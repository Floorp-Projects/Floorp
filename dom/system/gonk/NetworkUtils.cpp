/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NetworkUtils.h"

#include <android/log.h>
#include <cutils/properties.h>
#include <limits>
#include "mozilla/dom/network/NetUtils.h"

#include <sys/types.h>  // struct addrinfo
#include <sys/socket.h> // getaddrinfo(), freeaddrinfo()
#include <netdb.h>
#include <arpa/inet.h>  // inet_ntop()

#define _DEBUG 0

#define WARN(args...)   __android_log_print(ANDROID_LOG_WARN,  "NetworkUtils", ## args)
#define ERROR(args...)  __android_log_print(ANDROID_LOG_ERROR,  "NetworkUtils", ## args)

#if _DEBUG
#define DEBUG(args...)  __android_log_print(ANDROID_LOG_DEBUG, "NetworkUtils" , ## args)
#else
#define DEBUG(args...)
#endif

using namespace mozilla::dom;
using namespace mozilla::ipc;

static const char* PERSIST_SYS_USB_CONFIG_PROPERTY = "persist.sys.usb.config";
static const char* SYS_USB_CONFIG_PROPERTY         = "sys.usb.config";
static const char* SYS_USB_STATE_PROPERTY          = "sys.usb.state";

static const char* USB_FUNCTION_RNDIS  = "rndis";
static const char* USB_FUNCTION_ADB    = "adb";

// Use this command to continue the function chain.
static const char* DUMMY_COMMAND = "tether status";

// Retry 20 times (2 seconds) for usb state transition.
static const uint32_t USB_FUNCTION_RETRY_TIMES = 20;
// Check "sys.usb.state" every 100ms.
static const uint32_t USB_FUNCTION_RETRY_INTERVAL = 100;

// 1xx - Requested action is proceeding
static const uint32_t NETD_COMMAND_PROCEEDING   = 100;
// 2xx - Requested action has been successfully completed
static const uint32_t NETD_COMMAND_OKAY         = 200;
// 4xx - The command is accepted but the requested action didn't
// take place.
static const uint32_t NETD_COMMAND_FAIL         = 400;
// 5xx - The command syntax or parameters error
static const uint32_t NETD_COMMAND_ERROR        = 500;
// 6xx - Unsolicited broadcasts
static const uint32_t NETD_COMMAND_UNSOLICITED  = 600;

// Broadcast messages
static const uint32_t NETD_COMMAND_INTERFACE_CHANGE     = 600;
static const uint32_t NETD_COMMAND_BANDWIDTH_CONTROLLER = 601;

static const char* INTERFACE_DELIMIT = ",";
static const char* USB_CONFIG_DELIMIT = ",";
static const char* NETD_MESSAGE_DELIMIT = " ";

static const uint32_t BUF_SIZE = 1024;

static uint32_t SDK_VERSION;

struct IFProperties {
  char gateway[PROPERTY_VALUE_MAX];
  char dns1[PROPERTY_VALUE_MAX];
  char dns2[PROPERTY_VALUE_MAX];
};

struct CurrentCommand {
  CommandChain* chain;
  CommandCallback callback;
  char command[MAX_COMMAND_SIZE];
};

typedef Tuple3<NetdCommand*, CommandChain*, CommandCallback> QueueData;

#define GET_CURRENT_NETD_COMMAND   (gCommandQueue.IsEmpty() ? nullptr : gCommandQueue[0].a)
#define GET_CURRENT_CHAIN          (gCommandQueue.IsEmpty() ? nullptr : gCommandQueue[0].b)
#define GET_CURRENT_CALLBACK       (gCommandQueue.IsEmpty() ? nullptr : gCommandQueue[0].c)
#define GET_CURRENT_COMMAND        (gCommandQueue.IsEmpty() ? nullptr : gCommandQueue[0].a->mData)

static NetworkUtils* gNetworkUtils;
static nsTArray<QueueData> gCommandQueue;
static CurrentCommand gCurrentCommand;
static bool gPending = false;
static nsTArray<nsCString> gReason;
static NetworkParams *gWifiTetheringParms = 0;


CommandFunc NetworkUtils::sWifiEnableChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::startAccessPointDriver,
  NetworkUtils::setAccessPoint,
  NetworkUtils::startSoftAP,
  NetworkUtils::setConfig,
  NetworkUtils::tetherInterface,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::tetheringStatus,
  NetworkUtils::startTethering,
  NetworkUtils::setDnsForwarders,
  NetworkUtils::enableNat,
  NetworkUtils::wifiTetheringSuccess
};

CommandFunc NetworkUtils::sWifiDisableChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::stopSoftAP,
  NetworkUtils::stopAccessPointDriver,
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::untetherInterface,
  NetworkUtils::preTetherInterfaceList,
  NetworkUtils::postTetherInterfaceList,
  NetworkUtils::disableNat,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering,
  NetworkUtils::wifiTetheringSuccess
};

CommandFunc NetworkUtils::sWifiFailChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::stopSoftAP,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering
};

CommandFunc NetworkUtils::sWifiRetryChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::stopSoftAP,
  NetworkUtils::stopTethering,

  // sWifiEnableChain:
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::startAccessPointDriver,
  NetworkUtils::setAccessPoint,
  NetworkUtils::startSoftAP,
  NetworkUtils::setConfig,
  NetworkUtils::tetherInterface,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::tetheringStatus,
  NetworkUtils::startTethering,
  NetworkUtils::setDnsForwarders,
  NetworkUtils::enableNat,
  NetworkUtils::wifiTetheringSuccess
};

CommandFunc NetworkUtils::sWifiOperationModeChain[] = {
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::wifiOperationModeSuccess
};

CommandFunc NetworkUtils::sUSBEnableChain[] = {
  NetworkUtils::setConfig,
  NetworkUtils::enableNat,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::tetherInterface,
  NetworkUtils::tetheringStatus,
  NetworkUtils::startTethering,
  NetworkUtils::setDnsForwarders,
  NetworkUtils::usbTetheringSuccess
};

CommandFunc NetworkUtils::sUSBDisableChain[] = {
  NetworkUtils::untetherInterface,
  NetworkUtils::preTetherInterfaceList,
  NetworkUtils::postTetherInterfaceList,
  NetworkUtils::disableNat,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering,
  NetworkUtils::usbTetheringSuccess
};

CommandFunc NetworkUtils::sUSBFailChain[] = {
  NetworkUtils::stopSoftAP,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering
};

CommandFunc NetworkUtils::sUpdateUpStreamChain[] = {
  NetworkUtils::cleanUpStream,
  NetworkUtils::createUpStream,
  NetworkUtils::updateUpStreamSuccess
};

CommandFunc NetworkUtils::sStartDhcpServerChain[] = {
  NetworkUtils::setConfig,
  NetworkUtils::startTethering,
  NetworkUtils::setDhcpServerSuccess
};

CommandFunc NetworkUtils::sStopDhcpServerChain[] = {
  NetworkUtils::stopTethering,
  NetworkUtils::setDhcpServerSuccess
};

CommandFunc NetworkUtils::sNetworkInterfaceStatsChain[] = {
  NetworkUtils::getRxBytes,
  NetworkUtils::getTxBytes,
  NetworkUtils::networkInterfaceStatsSuccess
};

CommandFunc NetworkUtils::sNetworkInterfaceEnableAlarmChain[] = {
  NetworkUtils::enableAlarm,
  NetworkUtils::setQuota,
  NetworkUtils::setAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

CommandFunc NetworkUtils::sNetworkInterfaceDisableAlarmChain[] = {
  NetworkUtils::removeQuota,
  NetworkUtils::disableAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

CommandFunc NetworkUtils::sNetworkInterfaceSetAlarmChain[] = {
  NetworkUtils::setAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

CommandFunc NetworkUtils::sSetDnsChain[] = {
  NetworkUtils::setDefaultInterface,
  NetworkUtils::setInterfaceDns
};

CommandFunc NetworkUtils::sGetInterfacesChain[] = {
  NetworkUtils::getInterfaceList,
  NetworkUtils::getInterfacesSuccess
};

CommandFunc NetworkUtils::sSetInterfaceConfigChain[] = {
  NetworkUtils::setConfig,
  NetworkUtils::setInterfaceConfigSuccess
};

CommandFunc NetworkUtils::sGetInterfaceConfigChain[] = {
  NetworkUtils::getConfig,
  NetworkUtils::getInterfaceConfigSuccess
};

/**
 * Helper function to get the mask from given prefix length.
 */
static uint32_t makeMask(const uint32_t prefixLength)
{
  uint32_t mask = 0;
  for (uint32_t i = 0; i < prefixLength; ++i) {
    mask |= (0x80000000 >> i);
  }
  return ntohl(mask);
}

/**
 * Helper function to get the network part of an ip from prefix.
 * param ip must be in network byte order.
 */
static char* getNetworkAddr(const uint32_t ip, const uint32_t prefix)
{
  uint32_t mask = 0, subnet = 0;

  mask = ~mask << (32 - prefix);
  mask = htonl(mask);
  subnet = ip & mask;

  struct in_addr addr;
  addr.s_addr = subnet;

  return inet_ntoa(addr);
}

/**
 * Helper function to split string by seperator, store split result as an nsTArray.
 */
static void split(char* str, const char* sep, nsTArray<nsCString>& result)
{
  char *s = strtok(str, sep);
  while (s != nullptr) {
    result.AppendElement(s);
    s = strtok(nullptr, sep);
  }
}

static void split(char* str, const char* sep, nsTArray<nsString>& result)
{
  char *s = strtok(str, sep);
  while (s != nullptr) {
    result.AppendElement(NS_ConvertUTF8toUTF16(s));
    s = strtok(nullptr, sep);
  }
}

/**
 * Helper function that implement join function.
 */
static void join(nsTArray<nsCString>& array,
                 const char* sep,
                 const uint32_t maxlen,
                 char* result)
{
#define CHECK_LENGTH(len, add, max)  len += add;          \
                                     if (len > max - 1)   \
                                       return;            \

  uint32_t len = 0;
  uint32_t seplen = strlen(sep);

  if (array.Length() > 0) {
    CHECK_LENGTH(len, strlen(array[0].get()), maxlen)
    strcpy(result, array[0].get());

    for (uint32_t i = 1; i < array.Length(); i++) {
      CHECK_LENGTH(len, seplen, maxlen)
      strcat(result, sep);

      CHECK_LENGTH(len, strlen(array[i].get()), maxlen)
      strcat(result, array[i].get());
    }
  }

#undef CHECK_LEN
}

static void convertUTF8toUTF16(nsTArray<nsCString>& narrow,
                               nsTArray<nsString>& wide,
                               uint32_t length)
{
  for (uint32_t i = 0; i < length; i++) {
    wide.AppendElement(NS_ConvertUTF8toUTF16(narrow[i].get()));
  }
}

/**
 * Helper function to get network interface properties from the system property table.
 */
static void getIFProperties(const char* ifname, IFProperties& prop)
{
  char key[PROPERTY_KEY_MAX];
  snprintf(key, PROPERTY_KEY_MAX - 1, "net.%s.gw", ifname);
  property_get(key, prop.gateway, "");
  snprintf(key, PROPERTY_KEY_MAX - 1, "net.%s.dns1", ifname);
  property_get(key, prop.dns1, "");
  snprintf(key, PROPERTY_KEY_MAX - 1, "net.%s.dns2", ifname);
  property_get(key, prop.dns2, "");
}

static int getIpType(const char *aIp) {
  struct addrinfo hint, *ip_info = NULL;

  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_flags = AI_NUMERICHOST;

  if (getaddrinfo(aIp, NULL, &hint, &ip_info)) {
    return AF_UNSPEC;
  }

  int type = ip_info->ai_family;
  freeaddrinfo(ip_info);

  return type;
}

/**
 * Helper function to find the best match gateway. For now, return
 * the gateway that matches the address family passed.
 */
static uint32_t selectGateway(nsTArray<nsString>& gateways, int addrFamily)
{
  uint32_t length = gateways.Length();

  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoGateway(gateways[i]);
    if ((getIpType(autoGateway.get()) == AF_INET && addrFamily == AF_INET) ||
        (getIpType(autoGateway.get()) == AF_INET6 && addrFamily == AF_INET6)) {
      return i;
    }
  }
  return length; // invalid index.
}

static void postMessage(NetworkResultOptions& aResult)
{
  MOZ_ASSERT(gNetworkUtils);
  MOZ_ASSERT(gNetworkUtils->getMessageCallback());

  if (*(gNetworkUtils->getMessageCallback()))
    (*(gNetworkUtils->getMessageCallback()))(aResult);
}

static void postMessage(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  MOZ_ASSERT(gNetworkUtils);
  MOZ_ASSERT(gNetworkUtils->getMessageCallback());

  aResult.mId = aOptions.mId;

  if (*(gNetworkUtils->getMessageCallback()))
    (*(gNetworkUtils->getMessageCallback()))(aResult);
}

void NetworkUtils::next(CommandChain* aChain, bool aError, NetworkResultOptions& aResult)
{
  if (aError) {
    ErrorCallback onError = aChain->getErrorCallback();
    if(onError) {
      aResult.mError = true;
      (*onError)(aChain->getParams(), aResult);
    }
    delete aChain;
    return;
  }
  CommandFunc f = aChain->getNextCommand();
  if (!f) {
    delete aChain;
    return;
  }

  (*f)(aChain, next, aResult);
}

/**
 * Send command to netd.
 */
void NetworkUtils::nextNetdCommand()
{
  if (gCommandQueue.IsEmpty() || gPending) {
    return;
  }

  gCurrentCommand.chain = GET_CURRENT_CHAIN;
  gCurrentCommand.callback = GET_CURRENT_CALLBACK;
  snprintf(gCurrentCommand.command, MAX_COMMAND_SIZE - 1, "%s", GET_CURRENT_COMMAND);

  DEBUG("Sending \'%s\' command to netd.", gCurrentCommand.command);
  SendNetdCommand(GET_CURRENT_NETD_COMMAND);

  gCommandQueue.RemoveElementAt(0);
  gPending = true;
}

/**
 * Composite NetdCommand sent to netd
 *
 * @param aCommand  Command sent to netd to execute.
 * @param aChain    Store command chain data, ex. command parameter.
 * @param aCallback Callback function to be executed when the result of
 *                  this command is returned from netd.
 */
void NetworkUtils::doCommand(const char* aCommand, CommandChain* aChain, CommandCallback aCallback)
{
  DEBUG("Preparing to send \'%s\' command...", aCommand);

  NetdCommand* netdCommand = new NetdCommand();

  // Android JB version adds sequence number to netd command.
  if (SDK_VERSION >= 16) {
    snprintf((char*)netdCommand->mData, MAX_COMMAND_SIZE - 1, "0 %s", aCommand);
  } else {
    snprintf((char*)netdCommand->mData, MAX_COMMAND_SIZE - 1, "%s", aCommand);
  }
  netdCommand->mSize = strlen((char*)netdCommand->mData) + 1;

  gCommandQueue.AppendElement(QueueData(netdCommand, aChain, aCallback));

  nextNetdCommand();
}

/*
 * Netd command function
 */
#define GET_CHAR(prop) NS_ConvertUTF16toUTF8(aChain->getParams().prop).get()
#define GET_FIELD(prop) aChain->getParams().prop

void NetworkUtils::wifiFirmwareReload(CommandChain* aChain,
                                      CommandCallback aCallback,
                                      NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "softap fwreload %s %s", GET_CHAR(mIfname), GET_CHAR(mMode));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::startAccessPointDriver(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult)
{
  // Skip the command for sdk version >= 16.
  if (SDK_VERSION >= 16) {
    aResult.mResultCode = 0;
    aResult.mResultReason = NS_ConvertUTF8toUTF16("");
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "softap start %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::stopAccessPointDriver(CommandChain* aChain,
                                         CommandCallback aCallback,
                                         NetworkResultOptions& aResult)
{
  // Skip the command for sdk version >= 16.
  if (SDK_VERSION >= 16) {
    aResult.mResultCode = 0;
    aResult.mResultReason = NS_ConvertUTF8toUTF16("");
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "softap stop %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

/**
 * Command format for sdk version < 16
 *   Arguments:
 *     argv[2] - wlan interface
 *     argv[3] - SSID
 *     argv[4] - Security
 *     argv[5] - Key
 *     argv[6] - Channel
 *     argv[7] - Preamble
 *     argv[8] - Max SCB
 *
 * Command format for sdk version >= 16
 *   Arguments:
 *     argv[2] - wlan interface
 *     argv[3] - SSID
 *     argv[4] - Security
 *     argv[5] - Key
 *
 * Command format for sdk version >= 18
 *   Arguments:
 *      argv[2] - wlan interface
 *      argv[3] - SSID
 *      argv[4] - Broadcast/Hidden
 *      argv[5] - Channel
 *      argv[6] - Security
 *      argv[7] - Key
 */
void NetworkUtils::setAccessPoint(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  nsCString ssid(GET_CHAR(mSsid));
  nsCString key(GET_CHAR(mKey));

  escapeQuote(ssid);
  escapeQuote(key);

  if (SDK_VERSION >= 19) {
    snprintf(command, MAX_COMMAND_SIZE - 1, "softap set %s \"%s\" broadcast 6 %s \"%s\"",
                     GET_CHAR(mIfname),
                     ssid.get(),
                     GET_CHAR(mSecurity),
                     key.get());
  } else if (SDK_VERSION >= 16) {
    snprintf(command, MAX_COMMAND_SIZE - 1, "softap set %s \"%s\" %s \"%s\"",
                     GET_CHAR(mIfname),
                     ssid.get(),
                     GET_CHAR(mSecurity),
                     key.get());
  } else {
    snprintf(command, MAX_COMMAND_SIZE - 1, "softap set %s %s \"%s\" %s \"%s\" 6 0 8",
                     GET_CHAR(mIfname),
                     GET_CHAR(mWifictrlinterfacename),
                     ssid.get(),
                     GET_CHAR(mSecurity),
                     key.get());
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::cleanUpStream(CommandChain* aChain,
                                 CommandCallback aCallback,
                                 NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "nat disable %s %s 0", GET_CHAR(mPreInternalIfname), GET_CHAR(mPreExternalIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::createUpStream(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "nat enable %s %s 0", GET_CHAR(mCurInternalIfname), GET_CHAR(mCurExternalIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::startSoftAP(CommandChain* aChain,
                               CommandCallback aCallback,
                               NetworkResultOptions& aResult)
{
  const char* command= "softap startap";
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::stopSoftAP(CommandChain* aChain,
                              CommandCallback aCallback,
                              NetworkResultOptions& aResult)
{
  const char* command= "softap stopap";
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::clearWifiTetherParms(CommandChain* aChain,
                                        CommandCallback aCallback,
                                        NetworkResultOptions& aResult)
{
  delete gWifiTetheringParms;
  gWifiTetheringParms = 0;
  next(aChain, false, aResult);
}

void NetworkUtils::getRxBytes(CommandChain* aChain,
                              CommandCallback aCallback,
                              NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "interface readrxcounter %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::getTxBytes(CommandChain* aChain,
                              CommandCallback aCallback,
                              NetworkResultOptions& aResult)
{
  NetworkParams& options = aChain->getParams();
  options.mRxBytes = atof(NS_ConvertUTF16toUTF8(aResult.mResultReason).get());

  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "interface readtxcounter %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::enableAlarm(CommandChain* aChain,
                               CommandCallback aCallback,
                               NetworkResultOptions& aResult)
{
  const char* command= "bandwidth enable";
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::disableAlarm(CommandChain* aChain,
                                CommandCallback aCallback,
                                NetworkResultOptions& aResult)
{
  const char* command= "bandwidth disable";
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setQuota(CommandChain* aChain,
                            CommandCallback aCallback,
                            NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth setiquota %s %lld", GET_CHAR(mIfname), LLONG_MAX);

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeQuota(CommandChain* aChain,
                               CommandCallback aCallback,
                               NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth removeiquota %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setAlarm(CommandChain* aChain,
                            CommandCallback aCallback,
                            NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth setinterfacealert %s %ld", GET_CHAR(mIfname), GET_FIELD(mThreshold));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setConfig(CommandChain* aChain,
                             CommandCallback aCallback,
                             NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  if (SDK_VERSION >= 16) {
    snprintf(command, MAX_COMMAND_SIZE - 1, "interface setcfg %s %s %s %s",
                     GET_CHAR(mIfname),
                     GET_CHAR(mIp),
                     GET_CHAR(mPrefix),
                     GET_CHAR(mLink));
  } else {
    snprintf(command, MAX_COMMAND_SIZE - 1, "interface setcfg %s %s %s [%s]",
                     GET_CHAR(mIfname),
                     GET_CHAR(mIp),
                     GET_CHAR(mPrefix),
                     GET_CHAR(mLink));
  }
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::tetherInterface(CommandChain* aChain,
                                   CommandCallback aCallback,
                                   NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface add %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::preTetherInterfaceList(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  if (SDK_VERSION >= 16) {
    snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface list");
  } else {
    snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface list 0");
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::postTetherInterfaceList(CommandChain* aChain,
                                           CommandCallback aCallback,
                                           NetworkResultOptions& aResult)
{
  // Send the dummy command to continue the function chain.
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);

  char buf[BUF_SIZE];
  const char* reason = NS_ConvertUTF16toUTF8(aResult.mResultReason).get();
  memcpy(buf, reason, strlen(reason));
  split(buf, INTERFACE_DELIMIT, GET_FIELD(mInterfaceList));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setIpForwardingEnabled(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  if (GET_FIELD(mEnable)) {
    snprintf(command, MAX_COMMAND_SIZE - 1, "ipfwd enable");
  } else {
    // Don't disable ip forwarding because others interface still need it.
    // Send the dummy command to continue the function chain.
    if (GET_FIELD(mInterfaceList).Length() > 1) {
      snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);
    } else {
      snprintf(command, MAX_COMMAND_SIZE - 1, "ipfwd disable");
    }
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::tetheringStatus(CommandChain* aChain,
                                   CommandCallback aCallback,
                                   NetworkResultOptions& aResult)
{
  const char* command= "tether status";
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::stopTethering(CommandChain* aChain,
                                 CommandCallback aCallback,
                                 NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  // Don't stop tethering because others interface still need it.
  // Send the dummy to continue the function chain.
  if (GET_FIELD(mInterfaceList).Length() > 1) {
    snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);
  } else {
    snprintf(command, MAX_COMMAND_SIZE - 1, "tether stop");
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::startTethering(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  // We don't need to start tethering again.
  // Send the dummy command to continue the function chain.
  if (aResult.mResultReason.Find("started") != kNotFound) {
    snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);
  } else {
    snprintf(command, MAX_COMMAND_SIZE - 1, "tether start %s %s", GET_CHAR(mWifiStartIp), GET_CHAR(mWifiEndIp));

    // If usbStartIp/usbEndIp is not valid, don't append them since
    // the trailing white spaces will be parsed to extra empty args
    // See: http://androidxref.com/4.3_r2.1/xref/system/core/libsysutils/src/FrameworkListener.cpp#78
    if (!GET_FIELD(mUsbStartIp).IsEmpty() && !GET_FIELD(mUsbEndIp).IsEmpty()) {
      snprintf(command, MAX_COMMAND_SIZE - 1, "%s %s %s", command, GET_CHAR(mUsbStartIp), GET_CHAR(mUsbEndIp));
    }
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::untetherInterface(CommandChain* aChain,
                                     CommandCallback aCallback,
                                     NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface remove %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setDnsForwarders(CommandChain* aChain,
                                    CommandCallback aCallback,
                                    NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "tether dns set %s %s", GET_CHAR(mDns1), GET_CHAR(mDns2));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::enableNat(CommandChain* aChain,
                             CommandCallback aCallback,
                             NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  if (!GET_FIELD(mIp).IsEmpty() && !GET_FIELD(mPrefix).IsEmpty()) {
    uint32_t prefix = atoi(GET_CHAR(mPrefix));
    uint32_t ip = inet_addr(GET_CHAR(mIp));
    char* networkAddr = getNetworkAddr(ip, prefix);

    // address/prefix will only take effect when secondary routing table exists.
    snprintf(command, MAX_COMMAND_SIZE - 1, "nat enable %s %s 1 %s/%s",
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname), networkAddr,
      GET_CHAR(mPrefix));
  } else {
    snprintf(command, MAX_COMMAND_SIZE - 1, "nat enable %s %s 0",
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::disableNat(CommandChain* aChain,
                              CommandCallback aCallback,
                              NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  if (!GET_FIELD(mIp).IsEmpty() && !GET_FIELD(mPrefix).IsEmpty()) {
    uint32_t prefix = atoi(GET_CHAR(mPrefix));
    uint32_t ip = inet_addr(GET_CHAR(mIp));
    char* networkAddr = getNetworkAddr(ip, prefix);

    snprintf(command, MAX_COMMAND_SIZE - 1, "nat disable %s %s 1 %s/%s",
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname), networkAddr,
      GET_CHAR(mPrefix));
  } else {
    snprintf(command, MAX_COMMAND_SIZE - 1, "nat disable %s %s 0",
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setDefaultInterface(CommandChain* aChain,
                                       CommandCallback aCallback,
                                       NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "resolver setdefaultif %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setInterfaceDns(CommandChain* aChain,
                                   CommandCallback aCallback,
                                   NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  int written = snprintf(command, sizeof command, "resolver setifdns %s %s",
                         GET_CHAR(mIfname), GET_CHAR(mDomain));

  nsTArray<nsString>& dnses = GET_FIELD(mDnses);
  uint32_t length = dnses.Length();

  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoDns(dnses[i]);

    int ret = snprintf(command + written, sizeof(command) - written, " %s", autoDns.get());
    if (ret <= 1) {
      command[written] = '\0';
      continue;
    }

    if ((ret + written) >= sizeof(command)) {
      command[written] = '\0';
      break;
    }

    written += ret;
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::getInterfaceList(CommandChain* aChain,
                                    CommandCallback aCallback,
                                    NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "interface list");

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::getConfig(CommandChain* aChain,
                             CommandCallback aCallback,
                             NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1, "interface getcfg %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

#undef GET_CHAR
#undef GET_FIELD

/*
 * Netd command success/fail function
 */
#define ASSIGN_FIELD(prop)  aResult.prop = aChain->getParams().prop;
#define ASSIGN_FIELD_VALUE(prop, value)  aResult.prop = value;

#define RUN_CHAIN(param, cmds, err)                                \
  uint32_t size = sizeof(cmds) / sizeof(CommandFunc);              \
  CommandChain* chain = new CommandChain(param, cmds, size, err);  \
  NetworkResultOptions result;                                     \
  next(chain, false, result);

void NetworkUtils::wifiTetheringFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  // Notify the main thread.
  postMessage(aOptions, aResult);

  // If one of the stages fails, we try roll back to ensure
  // we don't leave the network systems in limbo.
  ASSIGN_FIELD_VALUE(mEnable, false)
  RUN_CHAIN(aOptions, sWifiFailChain, nullptr)
}

void NetworkUtils::wifiTetheringSuccess(CommandChain* aChain,
                                        CommandCallback aCallback,
                                        NetworkResultOptions& aResult)
{
  ASSIGN_FIELD(mEnable)

  if (aChain->getParams().mEnable) {
    MOZ_ASSERT(!gWifiTetheringParms);
    gWifiTetheringParms = new NetworkParams(aChain->getParams());
  }
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::usbTetheringFail(NetworkParams& aOptions,
                                    NetworkResultOptions& aResult)
{
  // Notify the main thread.
  postMessage(aOptions, aResult);
  // Try to roll back to ensure
  // we don't leave the network systems in limbo.
  // This parameter is used to disable ipforwarding.
  {
    aOptions.mEnable = false;
    RUN_CHAIN(aOptions, sUSBFailChain, nullptr)
  }

  // Disable usb rndis function.
  {
    NetworkParams options;
    options.mEnable = false;
    options.mReport = false;
    gNetworkUtils->enableUsbRndis(options);
  }
}

void NetworkUtils::usbTetheringSuccess(CommandChain* aChain,
                                       CommandCallback aCallback,
                                       NetworkResultOptions& aResult)
{
  ASSIGN_FIELD(mEnable)
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::networkInterfaceStatsFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::networkInterfaceStatsSuccess(CommandChain* aChain,
                                                CommandCallback aCallback,
                                                NetworkResultOptions& aResult)
{
  ASSIGN_FIELD(mRxBytes)
  ASSIGN_FIELD_VALUE(mTxBytes, atof(NS_ConvertUTF16toUTF8(aResult.mResultReason).get()))
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::networkInterfaceAlarmFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::networkInterfaceAlarmSuccess(CommandChain* aChain,
                                                CommandCallback aCallback,
                                                NetworkResultOptions& aResult)
{
  // TODO : error is not used , and it is conflict with boolean type error.
  // params.error = parseFloat(params.resultReason);
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::updateUpStreamFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::updateUpStreamSuccess(CommandChain* aChain,
                                         CommandCallback aCallback,
                                         NetworkResultOptions& aResult)
{
  ASSIGN_FIELD(mCurExternalIfname)
  ASSIGN_FIELD(mCurInternalIfname)
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::setDhcpServerFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  aResult.mSuccess = false;
  postMessage(aOptions, aResult);
}

void NetworkUtils::setDhcpServerSuccess(CommandChain* aChain, CommandCallback aCallback, NetworkResultOptions& aResult)
{
  aResult.mSuccess = true;
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::wifiOperationModeFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::wifiOperationModeSuccess(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult)
{
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::setDnsFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::getInterfacesFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::getInterfacesSuccess(CommandChain* aChain,
                                        CommandCallback aCallback,
                                        NetworkResultOptions& aResult)
{
  char buf[BUF_SIZE];
  NS_ConvertUTF16toUTF8 reason(aResult.mResultReason);
  memcpy(buf, reason.get(), strlen(reason.get()));

  nsTArray<nsCString> result;
  split(buf, INTERFACE_DELIMIT, result);

  nsTArray<nsString> interfaceList;
  uint32_t length = result.Length();
  convertUTF8toUTF16(result, interfaceList, length);

  aResult.mInterfaceList.Construct();
  for (uint32_t i = 0; i < length; i++) {
    aResult.mInterfaceList.Value().AppendElement(interfaceList[i]);
  }

  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::setInterfaceConfigFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::setInterfaceConfigSuccess(CommandChain* aChain,
                                             CommandCallback aCallback,
                                             NetworkResultOptions& aResult)
{
  postMessage(aChain->getParams(), aResult);
}

void NetworkUtils::getInterfaceConfigFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::getInterfaceConfigSuccess(CommandChain* aChain,
                                             CommandCallback aCallback,
                                             NetworkResultOptions& aResult)
{
  char buf[BUF_SIZE];
  NS_ConvertUTF16toUTF8 reason(aResult.mResultReason);
  memcpy(buf, reason.get(), strlen(reason.get()));

  nsTArray<nsCString> result;
  split(buf, NETD_MESSAGE_DELIMIT, result);

  ASSIGN_FIELD_VALUE(mMacAddr, NS_ConvertUTF8toUTF16(result[0]))
  ASSIGN_FIELD_VALUE(mIpAddr, NS_ConvertUTF8toUTF16(result[1]))
  ASSIGN_FIELD_VALUE(mMaskLength, atof(result[2].get()))

  if (result[3].Find("up")) {
    ASSIGN_FIELD_VALUE(mFlag, NS_ConvertUTF8toUTF16("up"))
  } else {
    ASSIGN_FIELD_VALUE(mFlag, NS_ConvertUTF8toUTF16("down"))
  }

  postMessage(aChain->getParams(), aResult);
}

#undef ASSIGN_FIELD
#undef ASSIGN_FIELD_VALUE

NetworkUtils::NetworkUtils(MessageCallback aCallback)
 : mMessageCallback(aCallback)
{
  mNetUtils = new NetUtils();

  char value[PROPERTY_VALUE_MAX];
  property_get("ro.build.version.sdk", value, nullptr);
  SDK_VERSION = atoi(value);

  gNetworkUtils = this;
}

NetworkUtils::~NetworkUtils()
{
}

#define GET_CHAR(prop) NS_ConvertUTF16toUTF8(aOptions.prop).get()
#define GET_FIELD(prop) aOptions.prop

void NetworkUtils::ExecuteCommand(NetworkParams aOptions)
{
  bool ret = true;

  if (aOptions.mCmd.EqualsLiteral("removeNetworkRoute")) {
    removeNetworkRoute(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setDNS")) {
    setDNS(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setDefaultRouteAndDNS")) {
    setDefaultRouteAndDNS(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("removeDefaultRoute")) {
    removeDefaultRoute(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("addHostRoute")) {
    addHostRoute(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("removeHostRoute")) {
    removeHostRoute(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("removeHostRoutes")) {
    removeHostRoutes(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("addSecondaryRoute")) {
    addSecondaryRoute(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("removeSecondaryRoute")) {
    removeSecondaryRoute(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("getNetworkInterfaceStats")) {
    getNetworkInterfaceStats(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setNetworkInterfaceAlarm")) {
    setNetworkInterfaceAlarm(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("enableNetworkInterfaceAlarm")) {
    enableNetworkInterfaceAlarm(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("disableNetworkInterfaceAlarm")) {
    disableNetworkInterfaceAlarm(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setWifiOperationMode")) {
    setWifiOperationMode(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setDhcpServer")) {
    setDhcpServer(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setWifiTethering")) {
    setWifiTethering(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setUSBTethering")) {
    setUSBTethering(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("enableUsbRndis")) {
    enableUsbRndis(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("updateUpStream")) {
    updateUpStream(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("getInterfaces")) {
    getInterfaces(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("stopDhcp")) {
    stopDhcp(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("setInterfaceConfig")) {
    setInterfaceConfig(aOptions);
  } else if (aOptions.mCmd.EqualsLiteral("getInterfaceConfig")) {
    getInterfaceConfig(aOptions);
  } else {
    WARN("unknon message");
    return;
  }

  if (!aOptions.mIsAsync) {
    NetworkResultOptions result;
    result.mRet = ret;
    postMessage(aOptions, result);
  }
}

/**
 * Handle received data from netd.
 */
void NetworkUtils::onNetdMessage(NetdCommand* aCommand)
{
  char* data = (char*)aCommand->mData;

  // get code & reason.
  char* result = strtok(data, NETD_MESSAGE_DELIMIT);

  if (!result) {
    nextNetdCommand();
    return;
  }
  uint32_t code = atoi(result);

  if (!isBroadcastMessage(code) && SDK_VERSION >= 16) {
    strtok(nullptr, NETD_MESSAGE_DELIMIT);
  }

  char* reason = strtok(nullptr, "\0");

  if (isBroadcastMessage(code)) {
    DEBUG("Receiving broadcast message from netd.");
    DEBUG("          ==> Code: %d  Reason: %s", code, reason);
    sendBroadcastMessage(code, reason);

    if (code == NETD_COMMAND_INTERFACE_CHANGE) {
      if (gWifiTetheringParms) {
        char linkdownReason[MAX_COMMAND_SIZE];
        snprintf(linkdownReason, MAX_COMMAND_SIZE - 1,
                 "Iface linkstate %s down",
                 NS_ConvertUTF16toUTF8(gWifiTetheringParms->mIfname).get());

        if (!strcmp(reason, linkdownReason)) {
          DEBUG("Wifi link down, restarting tethering.");
          RUN_CHAIN(*gWifiTetheringParms, sWifiRetryChain, wifiTetheringFail)
        }
      }
    }

    nextNetdCommand();
    return;
  }

   // Set pending to false before we handle next command.
  DEBUG("Receiving \"%s\" command response from netd.", gCurrentCommand.command);
  DEBUG("          ==> Code: %d  Reason: %s", code, reason);

  gReason.AppendElement(nsCString(reason));

  // 1xx response code regards as command is proceeding, we need to wait for
  // final response code such as 2xx, 4xx and 5xx before sending next command.
  if (isProceeding(code)) {
    return;
  }

  if (isComplete(code)) {
    gPending = false;
  }

  if (gCurrentCommand.callback) {
    char buf[BUF_SIZE];
    join(gReason, INTERFACE_DELIMIT, BUF_SIZE, buf);

    NetworkResultOptions result;
    result.mResultCode = code;
    result.mResultReason = NS_ConvertUTF8toUTF16(buf);
    (*gCurrentCommand.callback)(gCurrentCommand.chain, isError(code), result);
    gReason.Clear();
  }

  // Handling pending commands if any.
  if (isComplete(code)) {
    nextNetdCommand();
  }
}

/**
 * Start/Stop DHCP server.
 */
bool NetworkUtils::setDhcpServer(NetworkParams& aOptions)
{
  if (aOptions.mEnabled) {
    aOptions.mWifiStartIp = aOptions.mStartIp;
    aOptions.mWifiEndIp = aOptions.mEndIp;
    aOptions.mIp = aOptions.mServerIp;
    aOptions.mPrefix = aOptions.mMaskLength;
    aOptions.mLink = NS_ConvertUTF8toUTF16("up");

    RUN_CHAIN(aOptions, sStartDhcpServerChain, setDhcpServerFail)
  } else {
    RUN_CHAIN(aOptions, sStopDhcpServerChain, setDhcpServerFail)
  }
  return true;
}

/**
 * Set DNS servers for given network interface.
 */
bool NetworkUtils::setDNS(NetworkParams& aOptions)
{
  uint32_t length = aOptions.mDnses.Length();

  if (length > 0) {
    for (uint32_t i = 0; i < length; i++) {
      NS_ConvertUTF16toUTF8 autoDns(aOptions.mDnses[i]);

      char dns_prop_key[PROPERTY_VALUE_MAX];
      snprintf(dns_prop_key, sizeof dns_prop_key, "net.dns%d", i+1);
      property_set(dns_prop_key, autoDns.get());
    }
  } else {
    // Set dnses from system properties.
    IFProperties interfaceProperties;
    getIFProperties(GET_CHAR(mIfname), interfaceProperties);

    property_set("net.dns1", interfaceProperties.dns1);
    property_set("net.dns2", interfaceProperties.dns2);
  }

  // Bump the DNS change property.
  char dnschange[PROPERTY_VALUE_MAX];
  property_get("net.dnschange", dnschange, "0");

  char num[PROPERTY_VALUE_MAX];
  snprintf(num, PROPERTY_VALUE_MAX - 1, "%d", atoi(dnschange) + 1);
  property_set("net.dnschange", num);

  // DNS needs to be set through netd since JellyBean (4.3).
  if (SDK_VERSION >= 18) {
    RUN_CHAIN(aOptions, sSetDnsChain, setDnsFail)
  }

  return true;
}

/**
 * Set default route and DNS servers for given network interface.
 */
bool NetworkUtils::setDefaultRouteAndDNS(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);

  if (!aOptions.mOldIfname.IsEmpty()) {
    // Remove IPv4's default route.
    mNetUtils->do_ifc_remove_default_route(GET_CHAR(mOldIfname));
    // Remove IPv6's default route.
    mNetUtils->do_ifc_remove_route(GET_CHAR(mOldIfname), "::", 0, NULL);
  }

  uint32_t length = aOptions.mGateways.Length();
  if (length > 0) {
    for (uint32_t i = 0; i < length; i++) {
      NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateways[i]);

      int type = getIpType(autoGateway.get());
      if (type != AF_INET && type != AF_INET6) {
        continue;
      }

      if (type == AF_INET6) {
        mNetUtils->do_ifc_add_route(autoIfname.get(), "::", 0, autoGateway.get());
      } else { /* type == AF_INET */
        mNetUtils->do_ifc_set_default_route(autoIfname.get(), inet_addr(autoGateway.get()));
      }
    }
  } else {
    // Set default froute from system properties.
    char key[PROPERTY_KEY_MAX];
    char gateway[PROPERTY_KEY_MAX];

    snprintf(key, sizeof key - 1, "net.%s.gw", autoIfname.get());
    property_get(key, gateway, "");

    int type = getIpType(gateway);
    if (type != AF_INET && type != AF_INET6) {
      return false;
    }

    if (type == AF_INET6) {
      mNetUtils->do_ifc_add_route(autoIfname.get(), "::", 0, gateway);
    } else { /* type == AF_INET */
      mNetUtils->do_ifc_set_default_route(autoIfname.get(), inet_addr(gateway));
    }
  }

  setDNS(aOptions);
  return true;
}

/**
 * Remove default route for given network interface.
 */
bool NetworkUtils::removeDefaultRoute(NetworkParams& aOptions)
{
  uint32_t length = aOptions.mGateways.Length();
  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateways[i]);

    int type = getIpType(autoGateway.get());
    if (type != AF_INET && type != AF_INET6) {
      return false;
    }

    mNetUtils->do_ifc_remove_route(GET_CHAR(mIfname),
                                   type == AF_INET ? "0.0.0.0" : "::",
                                   0, autoGateway.get());
  }

  return true;
}

/**
 * Add host route for given network interface.
 */
bool NetworkUtils::addHostRoute(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  int type, prefix;

  uint32_t length = aOptions.mHostnames.Length();
  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoHostname(aOptions.mHostnames[i]);

    type = getIpType(autoHostname.get());
    if (type != AF_INET && type != AF_INET6) {
      continue;
    }

    uint32_t index = selectGateway(aOptions.mGateways, type);
    if (index >= aOptions.mGateways.Length()) {
      continue;
    }

    NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateways[index]);
    prefix = type == AF_INET ? 32 : 128;
    mNetUtils->do_ifc_add_route(autoIfname.get(), autoHostname.get(), prefix,
                                autoGateway.get());
  }
  return true;
}

/**
 * Remove host route for given network interface.
 */
bool NetworkUtils::removeHostRoute(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  int type, prefix;

  uint32_t length = aOptions.mHostnames.Length();
  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoHostname(aOptions.mHostnames[i]);

    type = getIpType(autoHostname.get());
    if (type != AF_INET && type != AF_INET6) {
      continue;
    }

    uint32_t index = selectGateway(aOptions.mGateways, type);
    if (index >= aOptions.mGateways.Length()) {
      continue;
    }

    NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateways[index]);
    prefix = type == AF_INET ? 32 : 128;
    mNetUtils->do_ifc_remove_route(autoIfname.get(), autoHostname.get(), prefix,
                                   autoGateway.get());
  }
  return true;
}

/**
 * Remove the routes associated with the named interface.
 */
bool NetworkUtils::removeHostRoutes(NetworkParams& aOptions)
{
  mNetUtils->do_ifc_remove_host_routes(GET_CHAR(mIfname));
  return true;
}

bool NetworkUtils::removeNetworkRoute(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  NS_ConvertUTF16toUTF8 autoIp(aOptions.mIp);

  int type = getIpType(autoIp.get());
  if (type != AF_INET && type != AF_INET6) {
    return false;
  }

  uint32_t prefixLength = GET_FIELD(mPrefixLength);

  if (type == AF_INET6) {
    // Calculate subnet.
    struct in6_addr in6;
    if (inet_pton(AF_INET6, autoIp.get(), &in6) != 1) {
      return false;
    }

    uint32_t p, i, p1, mask;
    p = prefixLength;
    i = 0;
    while (i < 4) {
      p1 = p > 32 ? 32 : p;
      p -= p1;
      mask = p1 ? ~0x0 << (32 - p1) : 0;
      in6.s6_addr32[i++] &= htonl(mask);
    }

    char subnetStr[INET6_ADDRSTRLEN];
    if (!inet_ntop(AF_INET6, &in6, subnetStr, sizeof subnetStr)) {
      return false;
    }

    // Remove default route.
    mNetUtils->do_ifc_remove_route(autoIfname.get(), "::", 0, NULL);

    // Remove subnet route.
    mNetUtils->do_ifc_remove_route(autoIfname.get(), subnetStr, prefixLength, NULL);
    return true;
  }

  /* type == AF_INET */
  uint32_t ip = inet_addr(autoIp.get());
  uint32_t netmask = makeMask(prefixLength);
  uint32_t subnet = ip & netmask;
  const char* gateway = "0.0.0.0";
  struct in_addr addr;
  addr.s_addr = subnet;
  const char* dst = inet_ntoa(addr);

  mNetUtils->do_ifc_remove_default_route(autoIfname.get());
  mNetUtils->do_ifc_remove_route(autoIfname.get(), dst, prefixLength, gateway);
  return true;
}

bool NetworkUtils::addSecondaryRoute(NetworkParams& aOptions)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1,
           "interface route add %s secondary %s %s %s",
           GET_CHAR(mIfname),
           GET_CHAR(mIp),
           GET_CHAR(mPrefix),
           GET_CHAR(mGateway));

  doCommand(command, nullptr, nullptr);
  return true;
}

bool NetworkUtils::removeSecondaryRoute(NetworkParams& aOptions)
{
  char command[MAX_COMMAND_SIZE];
  snprintf(command, MAX_COMMAND_SIZE - 1,
           "interface route remove %s secondary %s %s %s",
           GET_CHAR(mIfname),
           GET_CHAR(mIp),
           GET_CHAR(mPrefix),
           GET_CHAR(mGateway));

  doCommand(command, nullptr, nullptr);
  return true;
}

bool NetworkUtils::getNetworkInterfaceStats(NetworkParams& aOptions)
{
  DEBUG("getNetworkInterfaceStats: %s", GET_CHAR(mIfname));
  aOptions.mRxBytes = -1;
  aOptions.mTxBytes = -1;

  RUN_CHAIN(aOptions, sNetworkInterfaceStatsChain, networkInterfaceStatsFail);
  return  true;
}

bool NetworkUtils::setNetworkInterfaceAlarm(NetworkParams& aOptions)
{
  DEBUG("setNetworkInterfaceAlarms: %s", GET_CHAR(mIfname));
  RUN_CHAIN(aOptions, sNetworkInterfaceSetAlarmChain, networkInterfaceAlarmFail);
  return true;
}

bool NetworkUtils::enableNetworkInterfaceAlarm(NetworkParams& aOptions)
{
  DEBUG("enableNetworkInterfaceAlarm: %s", GET_CHAR(mIfname));
  RUN_CHAIN(aOptions, sNetworkInterfaceEnableAlarmChain, networkInterfaceAlarmFail);
  return true;
}

bool NetworkUtils::disableNetworkInterfaceAlarm(NetworkParams& aOptions)
{
  DEBUG("disableNetworkInterfaceAlarms: %s", GET_CHAR(mIfname));
  RUN_CHAIN(aOptions, sNetworkInterfaceDisableAlarmChain, networkInterfaceAlarmFail);
  return true;
}

/**
 * handling main thread's reload Wifi firmware request
 */
bool NetworkUtils::setWifiOperationMode(NetworkParams& aOptions)
{
  DEBUG("setWifiOperationMode: %s %s", GET_CHAR(mIfname), GET_CHAR(mMode));
  RUN_CHAIN(aOptions, sWifiOperationModeChain, wifiOperationModeFail);
  return true;
}

/**
 * handling main thread's enable/disable WiFi Tethering request
 */
bool NetworkUtils::setWifiTethering(NetworkParams& aOptions)
{
  bool enable = aOptions.mEnable;
  IFProperties interfaceProperties;
  getIFProperties(GET_CHAR(mExternalIfname), interfaceProperties);

  if (strcmp(interfaceProperties.dns1, "")) {
    int type = getIpType(interfaceProperties.dns1);
    if (type != AF_INET6) {
      aOptions.mDns1 = NS_ConvertUTF8toUTF16(interfaceProperties.dns1);
    }
  }
  if (strcmp(interfaceProperties.dns2, "")) {
    int type = getIpType(interfaceProperties.dns2);
    if (type != AF_INET6) {
      aOptions.mDns2 = NS_ConvertUTF8toUTF16(interfaceProperties.dns2);
    }
  }
  dumpParams(aOptions, "WIFI");

  if (enable) {
    DEBUG("Starting Wifi Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    RUN_CHAIN(aOptions, sWifiEnableChain, wifiTetheringFail)
  } else {
    DEBUG("Stopping Wifi Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    RUN_CHAIN(aOptions, sWifiDisableChain, wifiTetheringFail)
  }
  return true;
}

bool NetworkUtils::setUSBTethering(NetworkParams& aOptions)
{
  bool enable = aOptions.mEnable;
  IFProperties interfaceProperties;
  getIFProperties(GET_CHAR(mExternalIfname), interfaceProperties);

  if (strcmp(interfaceProperties.dns1, "")) {
    int type = getIpType(interfaceProperties.dns1);
    if (type != AF_INET6) {
      aOptions.mDns1 = NS_ConvertUTF8toUTF16(interfaceProperties.dns1);
    }
  }
  if (strcmp(interfaceProperties.dns2, "")) {
    int type = getIpType(interfaceProperties.dns2);
    if (type != AF_INET6) {
      aOptions.mDns2 = NS_ConvertUTF8toUTF16(interfaceProperties.dns2);
    }
  }
  dumpParams(aOptions, "USB");

  if (enable) {
    DEBUG("Starting USB Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    RUN_CHAIN(aOptions, sUSBEnableChain, usbTetheringFail)
  } else {
    DEBUG("Stopping USB Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    RUN_CHAIN(aOptions, sUSBDisableChain, usbTetheringFail)
  }
  return true;
}

void NetworkUtils::escapeQuote(nsCString& aString)
{
  aString.ReplaceSubstring("\\", "\\\\");
  aString.ReplaceSubstring("\"", "\\\"");
}

void NetworkUtils::checkUsbRndisState(NetworkParams& aOptions)
{
  static uint32_t retry = 0;

  char currentState[PROPERTY_VALUE_MAX];
  property_get(SYS_USB_STATE_PROPERTY, currentState, nullptr);

  nsTArray<nsCString> stateFuncs;
  split(currentState, USB_CONFIG_DELIMIT, stateFuncs);
  bool rndisPresent = stateFuncs.Contains(nsCString(USB_FUNCTION_RNDIS));

  if (aOptions.mEnable == rndisPresent) {
    NetworkResultOptions result;
    result.mEnable = aOptions.mEnable;
    result.mResult = true;
    postMessage(aOptions, result);
    retry = 0;
    return;
  }
  if (retry < USB_FUNCTION_RETRY_TIMES) {
    retry++;
    usleep(USB_FUNCTION_RETRY_INTERVAL * 1000);
    checkUsbRndisState(aOptions);
    return;
  }

  NetworkResultOptions result;
  result.mResult = false;
  postMessage(aOptions, result);
  retry = 0;
}

/**
 * Modify usb function's property to turn on USB RNDIS function
 */
bool NetworkUtils::enableUsbRndis(NetworkParams& aOptions)
{
  bool report = aOptions.mReport;

  // For some reason, rndis doesn't play well with diag,modem,nmea.
  // So when turning rndis on, we set sys.usb.config to either "rndis"
  // or "rndis,adb". When turning rndis off, we go back to
  // persist.sys.usb.config.
  //
  // On the otoro/unagi, persist.sys.usb.config should be one of:
  //
  //    diag,modem,nmea,mass_storage
  //    diag,modem,nmea,mass_storage,adb
  //
  // When rndis is enabled, sys.usb.config should be one of:
  //
  //    rdnis
  //    rndis,adb
  //
  // and when rndis is disabled, it should revert to persist.sys.usb.config

  char currentConfig[PROPERTY_VALUE_MAX];
  property_get(SYS_USB_CONFIG_PROPERTY, currentConfig, nullptr);

  nsTArray<nsCString> configFuncs;
  split(currentConfig, USB_CONFIG_DELIMIT, configFuncs);

  char persistConfig[PROPERTY_VALUE_MAX];
  property_get(PERSIST_SYS_USB_CONFIG_PROPERTY, persistConfig, nullptr);

  nsTArray<nsCString> persistFuncs;
  split(persistConfig, USB_CONFIG_DELIMIT, persistFuncs);

  if (aOptions.mEnable) {
    configFuncs.Clear();
    configFuncs.AppendElement(nsCString(USB_FUNCTION_RNDIS));
    if (persistFuncs.Contains(nsCString(USB_FUNCTION_ADB))) {
      configFuncs.AppendElement(nsCString(USB_FUNCTION_ADB));
    }
  } else {
    // We're turning rndis off, revert back to the persist setting.
    // adb will already be correct there, so we don't need to do any
    // further adjustments.
    configFuncs = persistFuncs;
  }

  char newConfig[PROPERTY_VALUE_MAX] = "";
  property_get(SYS_USB_CONFIG_PROPERTY, currentConfig, nullptr);
  join(configFuncs, USB_CONFIG_DELIMIT, PROPERTY_VALUE_MAX, newConfig);
  if (strcmp(currentConfig, newConfig)) {
    property_set(SYS_USB_CONFIG_PROPERTY, newConfig);
  }

  // Trigger the timer to check usb state and report the result to NetworkManager.
  if (report) {
    usleep(USB_FUNCTION_RETRY_INTERVAL * 1000);
    checkUsbRndisState(aOptions);
  }
  return true;
}

/**
 * Handling upstream interface change event.
 */
bool NetworkUtils::updateUpStream(NetworkParams& aOptions)
{
  RUN_CHAIN(aOptions, sUpdateUpStreamChain, updateUpStreamFail)
  return true;
}

/**
 * Stop dhcp client deamon.
 */
bool NetworkUtils::stopDhcp(NetworkParams& aOptions)
{
  mNetUtils->do_dhcp_stop(GET_CHAR(mIfname));
  return true;
}

/**
 * Get existing network interfaces.
 */
bool NetworkUtils::getInterfaces(NetworkParams& aOptions)
{
  RUN_CHAIN(aOptions, sGetInterfacesChain, getInterfacesFail)
  return true;
}

/**
 * Set network config for a specified interface.
 */
bool NetworkUtils::setInterfaceConfig(NetworkParams& aOptions)
{
  RUN_CHAIN(aOptions, sSetInterfaceConfigChain, setInterfaceConfigFail)
  return true;
}

/**
 * Get network config of a specified interface.
 */
bool NetworkUtils::getInterfaceConfig(NetworkParams& aOptions)
{
  RUN_CHAIN(aOptions, sGetInterfaceConfigChain, getInterfaceConfigFail)
  return true;
}

void NetworkUtils::sendBroadcastMessage(uint32_t code, char* reason)
{
  NetworkResultOptions result;
  switch(code) {
    case NETD_COMMAND_INTERFACE_CHANGE:
      result.mTopic = NS_ConvertUTF8toUTF16("netd-interface-change");
      break;
    case NETD_COMMAND_BANDWIDTH_CONTROLLER:
      result.mTopic = NS_ConvertUTF8toUTF16("netd-bandwidth-control");
      break;
    default:
      return;
  }

  result.mBroadcast = true;
  result.mReason = NS_ConvertUTF8toUTF16(reason);
  postMessage(result);
}

inline uint32_t NetworkUtils::netdResponseType(uint32_t code)
{
  return (code / 100) * 100;
}

inline bool NetworkUtils::isBroadcastMessage(uint32_t code)
{
  uint32_t type = netdResponseType(code);
  return type == NETD_COMMAND_UNSOLICITED;
}

inline bool NetworkUtils::isError(uint32_t code)
{
  uint32_t type = netdResponseType(code);
  return type != NETD_COMMAND_PROCEEDING && type != NETD_COMMAND_OKAY;
}

inline bool NetworkUtils::isComplete(uint32_t code)
{
  uint32_t type = netdResponseType(code);
  return type != NETD_COMMAND_PROCEEDING;
}

inline bool NetworkUtils::isProceeding(uint32_t code)
{
  uint32_t type = netdResponseType(code);
  return type == NETD_COMMAND_PROCEEDING;
}

void NetworkUtils::dumpParams(NetworkParams& aOptions, const char* aType)
{
#ifdef _DEBUG
  DEBUG("Dump params:");
  DEBUG("     ifname: %s", GET_CHAR(mIfname));
  DEBUG("     ip: %s", GET_CHAR(mIp));
  DEBUG("     link: %s", GET_CHAR(mLink));
  DEBUG("     prefix: %s", GET_CHAR(mPrefix));
  DEBUG("     wifiStartIp: %s", GET_CHAR(mWifiStartIp));
  DEBUG("     wifiEndIp: %s", GET_CHAR(mWifiEndIp));
  DEBUG("     usbStartIp: %s", GET_CHAR(mUsbStartIp));
  DEBUG("     usbEndIp: %s", GET_CHAR(mUsbEndIp));
  DEBUG("     dnsserver1: %s", GET_CHAR(mDns1));
  DEBUG("     dnsserver2: %s", GET_CHAR(mDns2));
  DEBUG("     internalIfname: %s", GET_CHAR(mInternalIfname));
  DEBUG("     externalIfname: %s", GET_CHAR(mExternalIfname));
  if (!strcmp(aType, "WIFI")) {
    DEBUG("     wifictrlinterfacename: %s", GET_CHAR(mWifictrlinterfacename));
    DEBUG("     ssid: %s", GET_CHAR(mSsid));
    DEBUG("     security: %s", GET_CHAR(mSecurity));
    DEBUG("     key: %s", GET_CHAR(mKey));
  }
#endif
}

#undef GET_CHAR
#undef GET_FIELD
