// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Version.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string.h> //for memset()
#include <signal.h>
#include <getopt.h>

std::string SoapySDRDeviceProbe(SoapySDR::Device *);

/***********************************************************************
 * Print help message
 **********************************************************************/
static int printHelp(void)
{
    std::cerr << "Usage SoapySDRUtil [options]" << std::endl;
    std::cerr << "  Options summary:" << std::endl;
    std::cerr << "    --help \t\t\t\t Print this help message" << std::endl;
    std::cerr << "    --info \t\t\t\t Print module information" << std::endl;
    std::cerr << "    --find[=\"driver=foo,type=bar\"] \t Discover available devices" << std::endl;
    std::cerr << "    --make[=\"driver=foo,type=bar\"] \t Create a device instance" << std::endl;
    std::cerr << "    --probe[=\"driver=foo,type=bar\"] \t Print detailed information" << std::endl;
    std::cerr << "    --check[=driverName] \t\t Check if driver is present" << std::endl;
    std::cerr << std::endl;
    return EXIT_SUCCESS;
}

/***********************************************************************
 * Print version and module info
 **********************************************************************/
static int printInfo(void)
{
    std::cerr << "API Version: v" << SoapySDR::getAPIVersion() << std::endl;
    std::cerr << "ABI Version: v" << SoapySDR::getABIVersion() << std::endl;
    std::cerr << "Install root: " << SoapySDR::getRootPath() << std::endl;

    const auto modules = SoapySDR::listModules();
    for (const auto &mod : modules) std::cerr << "Module found: " << mod << std::endl;
    if (modules.empty()) std::cerr << "No modules found!" << std::endl;

    std::cerr << "Loading modules... " << std::flush;
    SoapySDR::loadModules();
    std::cerr << "done" << std::endl;

    std::cerr << "Available factories...";
    const auto factories = SoapySDR::Registry::listFindFunctions();
    for (const auto &it : factories) std::cerr << it.first << ", ";
    if (factories.empty()) std::cerr << "No factories found!" << std::endl;
    std::cerr << std::endl;
    return EXIT_SUCCESS;
}

/***********************************************************************
 * Find devices and print args
 **********************************************************************/
static int findDevices(void)
{
    std::string argStr;
    if (optarg != NULL) argStr = optarg;

    const auto results = SoapySDR::Device::enumerate(argStr);
    for (size_t i = 0; i < results.size(); i++)
    {
        std::cerr << "Found device " << i << std::endl;
        for (const auto &it : results[i])
        {
            std::cerr << "  " << it.first << " = " << it.second << std::endl;
        }
        std::cerr << std::endl;
    }
    if (results.empty())
    {
        std::cerr << "No devices found!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cerr << std::endl;
    return EXIT_SUCCESS;
}

/***********************************************************************
 * Make device and print hardware info
 **********************************************************************/
static int makeDevice(void)
{
    std::string argStr;
    if (optarg != NULL) argStr = optarg;

    std::cerr << "Make device " << argStr << std::endl;
    try
    {
        auto device = SoapySDR::Device::make(argStr);
        std::cerr << "  driver=" << device->getDriverKey() << std::endl;
        std::cerr << "  hardware=" << device->getHardwareKey() << std::endl;
        for (const auto &it : device->getHardwareInfo())
        {
            std::cerr << "  " << it.first << "=" << it.second << std::endl;
        }
        SoapySDR::Device::unmake(device);
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error making device: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cerr << std::endl;
    return EXIT_SUCCESS;
}

/***********************************************************************
 * Make device and print detailed info
 **********************************************************************/
static int probeDevice(void)
{
    std::string argStr;
    if (optarg != NULL) argStr = optarg;

    std::cerr << "Probe device " << argStr << std::endl;
    try
    {
        auto device = SoapySDR::Device::make(argStr);
        std::cerr << SoapySDRDeviceProbe(device) << std::endl;
        SoapySDR::Device::unmake(device);
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error probing device: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cerr << std::endl;
    return EXIT_SUCCESS;
}

/***********************************************************************
 * Check the registry for a specific driver
 **********************************************************************/
static int checkDriver(void)
{
    std::string driverName;
    if (optarg != NULL) driverName = optarg;

    std::cerr << "Loading modules... " << std::flush;
    SoapySDR::loadModules();
    std::cerr << "done" << std::endl;

    std::cerr << "Checking driver '" << driverName << "'... " << std::flush;
    const auto factories = SoapySDR::Registry::listFindFunctions();

    if (factories.find(driverName) == factories.end())
    {
        std::cerr << "MISSING!" << std::endl;
        return EXIT_FAILURE;
    }
    else
    {
        std::cerr << "PRESENT" << std::endl;
        return EXIT_SUCCESS;
    }
}

size_t currentChannel = 0;
bool currentFrequencySet = false;
double currentFrequency = 0;
bool currentSampleRateSet = false;
double currentSampleRate = 0;
int currentBufferSize = 1024*96;
bool signalReceivedDoExit = false;

static void channel(void)
{
    std::string argStr;
    if (optarg != NULL) currentChannel = (size_t)atoi(optarg);
    else std::cerr << "Missing argument for --channel." << std::endl;
}

static void frequency(void)
{
    std::string argStr;
    if (optarg != NULL) { currentFrequency = atof(optarg); currentFrequencySet = true; }
    else std::cerr << "Missing argument for --frequency." << std::endl;
}

static void samplerate(void)
{
    std::string argStr;
    if (optarg != NULL) { currentSampleRate = atof(optarg); currentSampleRateSet = true; }
    else std::cerr << "Missing argument for --samplerate." << std::endl;
}

static void buffersize(void)
{
    std::string argStr;
    if (optarg != NULL) currentBufferSize = atoi(optarg);
    else std::cerr << "Missing argument for --buffersize." << std::endl;
}

/***********************************************************************
 * Set the device into receive mode and start streaming
 **********************************************************************/
static int receive(void)
{
    std::ostringstream errorStringStream;
    std::string argStr;
    if (optarg != NULL) argStr = optarg;

    std::cerr << "Receiving from device " << argStr << std::endl;
    try
    {
        auto device = SoapySDR::Device::make(argStr);
        std::cerr << SoapySDRDeviceProbe(device) << std::endl;

        //Sanity checks
        if(currentChannel >= device->getNumChannels(SOAPY_SDR_RX))
            throw std::runtime_error(std::string("invalid channel"));

        if(!currentFrequencySet) throw std::runtime_error(std::string("--frequency is missing"));
        SoapySDR::RangeList freqRanges = device->getFrequencyRange(SOAPY_SDR_RX, currentChannel);
        for(unsigned i=0;i<freqRanges.size();i++)
        {
            if(currentFrequency<freqRanges.at(i).minimum() || currentFrequency>freqRanges.at(i).maximum())
                throw std::runtime_error(std::string("frequency out of range, use --info to show valid range"));
        }

        if(!currentSampleRateSet) throw std::runtime_error(std::string("--samplerate is missing"));
        //// This works, butSample rate list is rather a range for SoapyRTLSDR [0.25, 3.2], so this is no good
        //std::vector<double> sampleRates = device->listSampleRates(SOAPY_SDR_RX, currentChannel);
        //bool foundSampleRate = false;
        //for(unsigned i=0;i<sampleRates.size();i++)
        //    if(currentSampleRate==sampleRates[i]) { foundSampleRate = true; break; }
        //if(!foundSampleRate) throw std::runtime_error(std::string("invalid sample rate, use --info to show valid sample rates"));
        if(currentSampleRate<=0) throw std::runtime_error(std::string("invalid sample rate"));

        //Set device
        device->setSampleRate(SOAPY_SDR_RX, currentChannel, currentSampleRate);
        device->setFrequency(SOAPY_SDR_RX, currentChannel, currentFrequency);

        double fullScale;
        std::string nativeStreamFormatStr = device->getNativeStreamFormat(SOAPY_SDR_RX, currentChannel, fullScale);
        int numBytesPerSample = SoapySDR::formatToSize(nativeStreamFormatStr);

        //std::cerr << "numBytesPerSample = " << numBytesPerSample << std::endl;

        std::vector<size_t> currentChannels;
        currentChannels.push_back(currentChannel);
        SoapySDR::Stream* receiveStream = device->setupStream(SOAPY_SDR_RX, nativeStreamFormatStr, currentChannels);

        int result = device->activateStream(receiveStream);
        if(result<0)
        {
            errorStringStream << "activateStream error " << result;
            throw std::runtime_error(errorStringStream.str());
        }

        void* buffer = malloc(numBytesPerSample*currentBufferSize);
        void* const bufferArray[1] = { buffer };
        //bufferArray[0]=buffer;

        while(true)
        {
            int flags = 0;
            long long timeNs = 0;
            int result = device->readStream(receiveStream, (void* const*)bufferArray, (size_t)currentBufferSize, flags, timeNs, 100000);
            if(signalReceivedDoExit) break;

            //some debug
            //unsigned char* ubuf = (unsigned char*)buffer;
            //int untildata = 0;
            //for(int i=0;i<currentBufferSize;i++)
            //{
            //    if (ubuf[i]==0 && ubuf[i+1]==0 && ubuf[i+2]==0) { untildata = i; break; }
            //}
            //std::cerr << "result: " << result << " untildata: " << untildata << std::endl;

            if(result>0) fwrite(buffer, result, numBytesPerSample, stdout);
            else if(result==0); //pass
            else if(result==-4); //std::cerr << "E";
            else
            {
                errorStringStream << "readStream error " << result;
                throw std::runtime_error(errorStringStream.str());
            }
        }

        device->deactivateStream(receiveStream);
        device->closeStream(receiveStream);
        SoapySDR::Device::unmake(device);
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error while receiving: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cerr << std::endl;
    return EXIT_SUCCESS;
}

void signalHandler(int whichSignal __attribute__((unused))) { signalReceivedDoExit = true; }

/***********************************************************************
 * main utility entry point
 **********************************************************************/
int main(int argc, char *argv[])
{
    std::cerr << "######################################################" << std::endl;
    std::cerr << "## Soapy SDR -- the SDR abstraction library" << std::endl;
    std::cerr << "######################################################" << std::endl;
    std::cerr << std::endl;

    //set signals
	struct sigaction sigAction;
	memset(&sigAction, 0, sizeof(sigAction));
	sigAction.sa_handler = signalHandler;
	sigaction(SIGTERM, &sigAction, NULL);
	sigaction(SIGQUIT, &sigAction, NULL);

    /*******************************************************************
     * parse command line options
     ******************************************************************/
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"find", optional_argument, 0, 'f'},
        {"make", optional_argument, 0, 'm'},
        {"info", optional_argument, 0, 'i'},
        {"probe", optional_argument, 0, 'p'},
        {"check", optional_argument, 0, 'c'},
        {"receive", optional_argument, 0, 'r'},
        {"channel", required_argument, 0, 'C'},
        {"frequency", required_argument, 0, 'F'},
        {"samplerate", required_argument, 0, 'S'},
        {"buffersize", required_argument, 0, 'B'},
        {0, 0, 0,  0}
    };
    int long_index = 0;
    int option = 0;
    while ((option = getopt_long(argc, argv, "hf:m:i:p:c:r:C:F:S:B:", long_options, &long_index)) != -1)
    {
        switch (option)
        {
        case 'h': return printHelp();
        case 'i': return printInfo();
        case 'f': return findDevices();
        case 'm': return makeDevice();
        case 'p': return probeDevice();
        case 'c': return checkDriver();
        case 'r': return receive();
        case 'C': channel(); break;
        case 'F': frequency(); break;
        case 'S': samplerate(); break;
        case 'B': buffersize(); break;
        }
    }

    //unknown or unspecified options, do help...
    return printHelp();
}
