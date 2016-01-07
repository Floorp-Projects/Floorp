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

#include "prprf.h"
#include "SystemProperty.h"

#include <android/log.h>
#include <limits>
#include "mozilla/dom/network/NetUtils.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>  // struct addrinfo
#include <sys/socket.h> // getaddrinfo(), freeaddrinfo()
#include <netdb.h>
#include <arpa/inet.h>  // inet_ntop()

#define _DEBUG 0

#define WARN(args...)   __android_log_print(ANDROID_LOG_WARN,  "NetworkUtils", ## args)
#define ERROR(args...)  __android_log_print(ANDROID_LOG_ERROR,  "NetworkUtils", ## args)

#if _DEBUG
#define NU_DBG(args...)  __android_log_print(ANDROID_LOG_DEBUG, "NetworkUtils" , ## args)
#else
#define NU_DBG(args...)
#endif

using namespace mozilla::dom;
using namespace mozilla::ipc;
using mozilla::system::Property;

static const char* PERSIST_SYS_USB_CONFIG_PROPERTY = "persist.sys.usb.config";
static const char* SYS_USB_CONFIG_PROPERTY         = "sys.usb.config";
static const char* SYS_USB_STATE_PROPERTY          = "sys.usb.state";

static const char* USB_FUNCTION_RNDIS  = "rndis";
static const char* USB_FUNCTION_ADB    = "adb";

// Use this command to continue the function chain.
static const char* DUMMY_COMMAND = "tether status";

// IPV6 Tethering is not supported in AOSP, use the property to
// identify vendor specific support in IPV6. We can remove this flag
// once upstream Android support IPV6 in tethering.
static const char* IPV6_TETHERING = "ro.tethering.ipv6";

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

static const int32_t SUCCESS = 0;

static uint32_t SDK_VERSION;
static uint32_t SUPPORT_IPV6_TETHERING;

struct IFProperties {
  char gateway[Property::VALUE_MAX_LENGTH];
  char dns1[Property::VALUE_MAX_LENGTH];
  char dns2[Property::VALUE_MAX_LENGTH];
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

// A macro for native function call return value check.
// For native function call, non-zero return value means failure.
#define RETURN_IF_FAILED(rv) do { \
  if (SUCCESS != rv) { \
    return rv; \
  } \
} while (0);

#define WARN_IF_FAILED(rv) do { \
  if (SUCCESS != rv) { \
    WARN("Error (%d) occurred in %s (%s:%d)", rv, __PRETTY_FUNCTION__, __FILE__, __LINE__); \
  } \
} while (0);

static NetworkUtils* gNetworkUtils;
static nsTArray<QueueData> gCommandQueue;
static CurrentCommand gCurrentCommand;
static bool gPending = false;
static nsTArray<nsCString> gReason;
static NetworkParams *gWifiTetheringParms = 0;

static nsTArray<CommandChain*> gCommandChainQueue;

const CommandFunc NetworkUtils::sWifiEnableChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::startAccessPointDriver,
  NetworkUtils::setAccessPoint,
  NetworkUtils::startSoftAP,
  NetworkUtils::setInterfaceUp,
  NetworkUtils::tetherInterface,
  NetworkUtils::addInterfaceToLocalNetwork,
  NetworkUtils::addRouteToLocalNetwork,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::tetheringStatus,
  NetworkUtils::startTethering,
  NetworkUtils::setDnsForwarders,
  NetworkUtils::enableNat,
  NetworkUtils::wifiTetheringSuccess
};

const CommandFunc NetworkUtils::sWifiDisableChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::stopSoftAP,
  NetworkUtils::stopAccessPointDriver,
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::untetherInterface,
  NetworkUtils::removeInterfaceFromLocalNetwork,
  NetworkUtils::preTetherInterfaceList,
  NetworkUtils::postTetherInterfaceList,
  NetworkUtils::disableNat,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering,
  NetworkUtils::wifiTetheringSuccess
};

const CommandFunc NetworkUtils::sWifiFailChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::stopSoftAP,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering
};

const CommandFunc NetworkUtils::sWifiRetryChain[] = {
  NetworkUtils::clearWifiTetherParms,
  NetworkUtils::stopSoftAP,
  NetworkUtils::stopTethering,

  // sWifiEnableChain:
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::startAccessPointDriver,
  NetworkUtils::setAccessPoint,
  NetworkUtils::startSoftAP,
  NetworkUtils::setInterfaceUp,
  NetworkUtils::tetherInterface,
  NetworkUtils::addInterfaceToLocalNetwork,
  NetworkUtils::addRouteToLocalNetwork,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::tetheringStatus,
  NetworkUtils::startTethering,
  NetworkUtils::setDnsForwarders,
  NetworkUtils::enableNat,
  NetworkUtils::wifiTetheringSuccess
};

const CommandFunc NetworkUtils::sWifiOperationModeChain[] = {
  NetworkUtils::wifiFirmwareReload,
  NetworkUtils::wifiOperationModeSuccess
};

const CommandFunc NetworkUtils::sUSBEnableChain[] = {
  NetworkUtils::setInterfaceUp,
  NetworkUtils::enableNat,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::tetherInterface,
  NetworkUtils::addInterfaceToLocalNetwork,
  NetworkUtils::addRouteToLocalNetwork,
  NetworkUtils::tetheringStatus,
  NetworkUtils::startTethering,
  NetworkUtils::setDnsForwarders,
  NetworkUtils::addUpstreamInterface,
  NetworkUtils::usbTetheringSuccess
};

const CommandFunc NetworkUtils::sUSBDisableChain[] = {
  NetworkUtils::untetherInterface,
  NetworkUtils::removeInterfaceFromLocalNetwork,
  NetworkUtils::preTetherInterfaceList,
  NetworkUtils::postTetherInterfaceList,
  NetworkUtils::removeUpstreamInterface,
  NetworkUtils::disableNat,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering,
  NetworkUtils::usbTetheringSuccess
};

const CommandFunc NetworkUtils::sUSBFailChain[] = {
  NetworkUtils::stopSoftAP,
  NetworkUtils::setIpForwardingEnabled,
  NetworkUtils::stopTethering
};

const CommandFunc NetworkUtils::sUpdateUpStreamChain[] = {
  NetworkUtils::cleanUpStream,
  NetworkUtils::removeUpstreamInterface,
  NetworkUtils::createUpStream,
  NetworkUtils::addUpstreamInterface,
  NetworkUtils::updateUpStreamSuccess
};

const CommandFunc NetworkUtils::sStartDhcpServerChain[] = {
  NetworkUtils::setInterfaceUp,
  NetworkUtils::startTethering,
  NetworkUtils::setDhcpServerSuccess
};

const CommandFunc NetworkUtils::sStopDhcpServerChain[] = {
  NetworkUtils::stopTethering,
  NetworkUtils::setDhcpServerSuccess
};

const CommandFunc NetworkUtils::sNetworkInterfaceEnableAlarmChain[] = {
  NetworkUtils::enableAlarm,
  NetworkUtils::setQuota,
  NetworkUtils::setAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

const CommandFunc NetworkUtils::sNetworkInterfaceDisableAlarmChain[] = {
  NetworkUtils::removeQuota,
  NetworkUtils::disableAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

const CommandFunc NetworkUtils::sNetworkInterfaceSetAlarmChain[] = {
  NetworkUtils::setAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

const CommandFunc NetworkUtils::sTetheringInterfaceSetAlarmChain[] = {
  NetworkUtils::setGlobalAlarm,
  NetworkUtils::removeAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

const CommandFunc NetworkUtils::sTetheringInterfaceRemoveAlarmChain[] = {
  NetworkUtils::removeGlobalAlarm,
  NetworkUtils::setAlarm,
  NetworkUtils::networkInterfaceAlarmSuccess
};

const CommandFunc NetworkUtils::sTetheringGetStatusChain[] = {
  NetworkUtils::tetheringStatus,
  NetworkUtils::defaultAsyncSuccessHandler
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

/**
 * Helper function to get network interface properties from the system property table.
 */
static void getIFProperties(const char* ifname, IFProperties& prop)
{
  char key[Property::KEY_MAX_LENGTH];
  PR_snprintf(key, Property::KEY_MAX_LENGTH - 1, "net.%s.gw", ifname);
  Property::Get(key, prop.gateway, "");
  PR_snprintf(key, Property::KEY_MAX_LENGTH - 1, "net.%s.dns1", ifname);
  Property::Get(key, prop.dns1, "");
  PR_snprintf(key, Property::KEY_MAX_LENGTH - 1, "net.%s.dns2", ifname);
  Property::Get(key, prop.dns2, "");
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

void NetworkUtils::runNextQueuedCommandChain()
{
  if (gCommandChainQueue.IsEmpty()) {
    NU_DBG("No command chain left in the queue. Done!");
    return;
  }
  NU_DBG("Process the queued command chain.");
  CommandChain* nextChain = gCommandChainQueue[0];
  NetworkResultOptions newResult;
  next(nextChain, false, newResult);
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
    gCommandChainQueue.RemoveElementAt(0);
    runNextQueuedCommandChain();
    return;
  }
  CommandFunc f = aChain->getNextCommand();
  if (!f) {
    delete aChain;
    gCommandChainQueue.RemoveElementAt(0);
    runNextQueuedCommandChain();
    return;
  }

  (*f)(aChain, next, aResult);
}

CommandResult::CommandResult(int32_t aResultCode)
  : mIsPending(false)
{
  // This is usually not a netd command. We treat the return code
  // typical linux convention, which uses 0 to indicate success.
  mResult.mError = (aResultCode == SUCCESS ? false : true);
  mResult.mResultCode = aResultCode;
  if (aResultCode != SUCCESS) {
    // The returned value is sometimes negative, make sure we pass a positive
    // error number to strerror.
    enum { STRERROR_R_BUF_SIZE = 1024, };
    char strerrorBuf[STRERROR_R_BUF_SIZE];
    strerror_r(abs(aResultCode), strerrorBuf, STRERROR_R_BUF_SIZE);
    mResult.mReason = NS_ConvertUTF8toUTF16(strerrorBuf);
  }
}

CommandResult::CommandResult(const mozilla::dom::NetworkResultOptions& aResult)
  : mResult(aResult)
  , mIsPending(false)
{
}

CommandResult::CommandResult(const Pending&)
  : mIsPending(true)
{
}

bool CommandResult::isPending() const
{
  return mIsPending;
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
  PR_snprintf(gCurrentCommand.command, MAX_COMMAND_SIZE - 1, "%s", GET_CURRENT_COMMAND);

  NU_DBG("Sending \'%s\' command to netd.", gCurrentCommand.command);
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
  NU_DBG("Preparing to send \'%s\' command...", aCommand);

  NetdCommand* netdCommand = new NetdCommand();

  // Android JB version adds sequence number to netd command.
  if (SDK_VERSION >= 16) {
    PR_snprintf((char*)netdCommand->mData, MAX_COMMAND_SIZE - 1, "0 %s", aCommand);
  } else {
    PR_snprintf((char*)netdCommand->mData, MAX_COMMAND_SIZE - 1, "%s", aCommand);
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
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "softap fwreload %s %s", GET_CHAR(mIfname), GET_CHAR(mMode));

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
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "softap start %s", GET_CHAR(mIfname));

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
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "softap stop %s", GET_CHAR(mIfname));

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
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "softap set %s \"%s\" broadcast 6 %s \"%s\"",
                     GET_CHAR(mIfname),
                     ssid.get(),
                     GET_CHAR(mSecurity),
                     key.get());
  } else if (SDK_VERSION >= 16) {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "softap set %s \"%s\" %s \"%s\"",
                     GET_CHAR(mIfname),
                     ssid.get(),
                     GET_CHAR(mSecurity),
                     key.get());
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "softap set %s %s \"%s\" %s \"%s\" 6 0 8",
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
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "nat disable %s %s 0", GET_CHAR(mPreInternalIfname), GET_CHAR(mPreExternalIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::createUpStream(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "nat enable %s %s 0", GET_CHAR(mCurInternalIfname), GET_CHAR(mCurExternalIfname));

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
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth setiquota %s %lld", GET_CHAR(mIfname), LLONG_MAX);

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeQuota(CommandChain* aChain,
                               CommandCallback aCallback,
                               NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth removeiquota %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setAlarm(CommandChain* aChain,
                            CommandCallback aCallback,
                            NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth setinterfacealert %s %lld", GET_CHAR(mIfname), GET_FIELD(mThreshold));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeAlarm(CommandChain* aChain,
                            CommandCallback aCallback,
                            NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth removeinterfacealert %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setGlobalAlarm(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth setglobalalert %ld", GET_FIELD(mThreshold));
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeGlobalAlarm(CommandChain* aChain,
                                     CommandCallback aCallback,
                                     NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "bandwidth removeglobalalert");
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setInterfaceUp(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  if (SDK_VERSION >= 16) {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "interface setcfg %s %s %s %s",
                     GET_CHAR(mIfname),
                     GET_CHAR(mIp),
                     GET_CHAR(mPrefix),
                     GET_CHAR(mLink));
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "interface setcfg %s %s %s [%s]",
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
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface add %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::addInterfaceToLocalNetwork(CommandChain* aChain,
                                              CommandCallback aCallback,
                                              NetworkResultOptions& aResult)
{
  // Skip the command for sdk version < 20.
  if (SDK_VERSION < 20) {
    aResult.mResultCode = 0;
    aResult.mResultReason = NS_ConvertUTF8toUTF16("");
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network interface add local %s",
              GET_CHAR(mInternalIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::addRouteToLocalNetwork(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult)
{
  // Skip the command for sdk version < 20.
  if (SDK_VERSION < 20) {
    aResult.mResultCode = 0;
    aResult.mResultReason = NS_ConvertUTF8toUTF16("");
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  uint32_t prefix = atoi(GET_CHAR(mPrefix));
  uint32_t ip = inet_addr(GET_CHAR(mIp));
  char* networkAddr = getNetworkAddr(ip, prefix);

  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network route add local %s %s/%s",
              GET_CHAR(mInternalIfname), networkAddr, GET_CHAR(mPrefix));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::preTetherInterfaceList(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  if (SDK_VERSION >= 16) {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface list");
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface list 0");
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::postTetherInterfaceList(CommandChain* aChain,
                                           CommandCallback aCallback,
                                           NetworkResultOptions& aResult)
{
  // Send the dummy command to continue the function chain.
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);

  char buf[BUF_SIZE];
  NS_ConvertUTF16toUTF8 reason(aResult.mResultReason);

  size_t length = reason.Length() + 1 < BUF_SIZE ? reason.Length() + 1 : BUF_SIZE;
  memcpy(buf, reason.get(), length);
  split(buf, INTERFACE_DELIMIT, GET_FIELD(mInterfaceList));

  doCommand(command, aChain, aCallback);
}

bool isCommandChainIPv6(CommandChain* aChain, const char *externalInterface) {
  // Check by gateway address
  if (getIpType(GET_CHAR(mGateway)) == AF_INET6) {
    return true;
  }

  uint32_t length = GET_FIELD(mGateways).Length();
  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoGateway(GET_FIELD(mGateways)[i]);
    if(getIpType(autoGateway.get()) == AF_INET6) {
      return true;
    }
  }

  // Check by external inteface address
  FILE *file = fopen("/proc/net/if_inet6", "r");
  if (!file) {
    return false;
  }

  bool isIPv6 = false;
  char interface[32];
  while(fscanf(file, "%*s %*s %*s %*s %*s %32s", interface)) {
    if (strcmp(interface, externalInterface) == 0) {
      isIPv6 = true;
      break;
    }
  }

  fclose(file);
  return isIPv6;
}

void NetworkUtils::addUpstreamInterface(CommandChain* aChain,
                                        CommandCallback aCallback,
                                        NetworkResultOptions& aResult)
{
  nsCString interface(GET_CHAR(mExternalIfname));
  if (!interface.get()[0]) {
    interface = GET_CHAR(mCurExternalIfname);
  }

  if (SUPPORT_IPV6_TETHERING == 0 || !isCommandChainIPv6(aChain, interface.get())) {
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface add_upstream %s",
              interface.get());
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeUpstreamInterface(CommandChain* aChain,
                                           CommandCallback aCallback,
                                           NetworkResultOptions& aResult)
{
  nsCString interface(GET_CHAR(mExternalIfname));
  if (!interface.get()[0]) {
    interface = GET_CHAR(mPreExternalIfname);
  }

  if (SUPPORT_IPV6_TETHERING == 0 || !isCommandChainIPv6(aChain, interface.get())) {
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface remove_upstream %s",
              interface.get());
  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setIpForwardingEnabled(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  if (GET_FIELD(mEnable)) {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "ipfwd enable");
  } else {
    // Don't disable ip forwarding because others interface still need it.
    // Send the dummy command to continue the function chain.
    if (GET_FIELD(mInterfaceList).Length() > 1) {
      PR_snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);
    } else {
      PR_snprintf(command, MAX_COMMAND_SIZE - 1, "ipfwd disable");
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
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether stop");
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
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "%s", DUMMY_COMMAND);
  } else {
    // If usbStartIp/usbEndIp is not valid, don't append them since
    // the trailing white spaces will be parsed to extra empty args
    // See: http://androidxref.com/4.3_r2.1/xref/system/core/libsysutils/src/FrameworkListener.cpp#78
    if (!GET_FIELD(mUsbStartIp).IsEmpty() && !GET_FIELD(mUsbEndIp).IsEmpty()) {
      PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether start %s %s %s %s",
                  GET_CHAR(mWifiStartIp), GET_CHAR(mWifiEndIp),
                  GET_CHAR(mUsbStartIp),  GET_CHAR(mUsbEndIp));
    } else {
      PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether start %s %s",
                  GET_CHAR(mWifiStartIp), GET_CHAR(mWifiEndIp));
    }
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::untetherInterface(CommandChain* aChain,
                                     CommandCallback aCallback,
                                     NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether interface remove %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeInterfaceFromLocalNetwork(CommandChain* aChain,
                                                   CommandCallback aCallback,
                                                   NetworkResultOptions& aResult)
{
  // Skip the command for sdk version < 20.
  if (SDK_VERSION < 20) {
    aResult.mResultCode = 0;
    aResult.mResultReason = NS_ConvertUTF8toUTF16("");
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network interface remove local %s",
              GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setDnsForwarders(CommandChain* aChain,
                                    CommandCallback aCallback,
                                    NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];

  if (SDK_VERSION >= 20) {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether dns set %d %s %s",
                GET_FIELD(mNetId), GET_CHAR(mDns1), GET_CHAR(mDns2));
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "tether dns set %s %s",
                GET_CHAR(mDns1), GET_CHAR(mDns2));
  }

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
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "nat enable %s %s 1 %s/%s",
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname), networkAddr,
      GET_CHAR(mPrefix));
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "nat enable %s %s 0",
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

    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "nat disable %s %s 1 %s/%s",
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname), networkAddr,
      GET_CHAR(mPrefix));
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1, "nat disable %s %s 0",
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setDefaultInterface(CommandChain* aChain,
                                       CommandCallback aCallback,
                                       NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "resolver setdefaultif %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeDefaultRoute(CommandChain* aChain,
                                      CommandCallback aCallback,
                                      NetworkResultOptions& aResult)
{
  if (GET_FIELD(mLoopIndex) >= GET_FIELD(mGateways).Length()) {
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  nsTArray<nsString>& gateways = GET_FIELD(mGateways);
  NS_ConvertUTF16toUTF8 autoGateway(gateways[GET_FIELD(mLoopIndex)]);

  int type = getIpType(autoGateway.get());
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network route remove %d %s %s/0 %s",
              GET_FIELD(mNetId), GET_CHAR(mIfname),
              type == AF_INET6 ? "::" : "0.0.0.0", autoGateway.get());

  struct MyCallback {
    static void callback(CommandCallback::CallbackType aOriginalCallback,
                         CommandChain* aChain,
                         bool aError,
                         mozilla::dom::NetworkResultOptions& aResult)
    {
      NS_ConvertUTF16toUTF8 reason(aResult.mResultReason);
      NU_DBG("removeDefaultRoute's reason: %s", reason.get());
      if (aError && !reason.EqualsASCII("removeRoute() failed (No such process)")) {
        return aOriginalCallback(aChain, aError, aResult);
      }

      GET_FIELD(mLoopIndex)++;
      return removeDefaultRoute(aChain, aOriginalCallback, aResult);
    }
  };

  CommandCallback wrappedCallback(MyCallback::callback, aCallback);
  doCommand(command, aChain, wrappedCallback);
}

void NetworkUtils::setInterfaceDns(CommandChain* aChain,
                                   CommandCallback aCallback,
                                   NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  int written;

  if (SDK_VERSION >= 20) {
    written = PR_snprintf(command, sizeof command, "resolver setnetdns %d %s",
                                                   GET_FIELD(mNetId), GET_CHAR(mDomain));
  } else {
    written = PR_snprintf(command, sizeof command, "resolver setifdns %s %s",
                                                   GET_CHAR(mIfname), GET_CHAR(mDomain));
  }

  nsTArray<nsString>& dnses = GET_FIELD(mDnses);
  uint32_t length = dnses.Length();

  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoDns(dnses[i]);

    int ret = PR_snprintf(command + written, sizeof(command) - written, " %s", autoDns.get());
    if (ret <= 1) {
      command[written] = '\0';
      continue;
    }

    if (((size_t)ret + written) >= sizeof(command)) {
      command[written] = '\0';
      break;
    }

    written += ret;
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::clearAddrForInterface(CommandChain* aChain,
                                         CommandCallback aCallback,
                                         NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "interface clearaddrs %s", GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::createNetwork(CommandChain* aChain,
                                 CommandCallback aCallback,
                                 NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network create %d", GET_FIELD(mNetId));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::destroyNetwork(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network destroy %d", GET_FIELD(mNetId));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::addInterfaceToNetwork(CommandChain* aChain,
                                         CommandCallback aCallback,
                                         NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network interface add %d %s",
                    GET_FIELD(mNetId), GET_CHAR(mIfname));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::addRouteToInterface(CommandChain* aChain,
                                       CommandCallback aCallback,
                                       NetworkResultOptions& aResult)
{
  struct MyCallback {
    static void callback(CommandCallback::CallbackType aOriginalCallback,
                         CommandChain* aChain,
                         bool aError,
                         mozilla::dom::NetworkResultOptions& aResult)
    {
      NS_ConvertUTF16toUTF8 reason(aResult.mResultReason);
      NU_DBG("addRouteToInterface's reason: %s", reason.get());
      if (aError && reason.EqualsASCII("addRoute() failed (File exists)")) {
        NU_DBG("Ignore \"File exists\" error when adding host route.");
        return aOriginalCallback(aChain, false, aResult);
      }
      aOriginalCallback(aChain, aError, aResult);
    }
  };

  CommandCallback wrappedCallback(MyCallback::callback, aCallback);
  modifyRouteOnInterface(aChain, wrappedCallback, aResult, true);
}

void NetworkUtils::removeRouteFromInterface(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult)
{
  modifyRouteOnInterface(aChain, aCallback, aResult, false);
}

void NetworkUtils::modifyRouteOnInterface(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult,
                                          bool aDoAdd)
{
  char command[MAX_COMMAND_SIZE];

  // AOSP adds host route to its interface table but it doesn't work for
  // B2G because we cannot set fwmark per application. So, we add
  // all host routes to legacy_system table except scope link route.

  nsCString ipOrSubnetIp = NS_ConvertUTF16toUTF8(GET_FIELD(mIp));
  nsCString gatewayOrEmpty;
  const char* legacyOrEmpty = "legacy 0 "; // Add to legacy by default.
  if (GET_FIELD(mGateway).IsEmpty()) {
    ipOrSubnetIp = getSubnetIp(ipOrSubnetIp, GET_FIELD(mPrefixLength));
    legacyOrEmpty = ""; // Add to interface table for scope link route.
  } else {
    gatewayOrEmpty = nsCString(" ") + NS_ConvertUTF16toUTF8(GET_FIELD(mGateway));
  }

  const char* action = aDoAdd ? "add" : "remove";

  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network route %s%s %d %s %s/%d%s",
              legacyOrEmpty, action,
              GET_FIELD(mNetId), GET_CHAR(mIfname), ipOrSubnetIp.get(),
              GET_FIELD(mPrefixLength), gatewayOrEmpty.get());

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::addDefaultRouteToNetwork(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult)
{
  if (GET_FIELD(mLoopIndex) >= GET_FIELD(mGateways).Length()) {
    aCallback(aChain, false, aResult);
    return;
  }

  char command[MAX_COMMAND_SIZE];
  nsTArray<nsString>& gateways = GET_FIELD(mGateways);
  NS_ConvertUTF16toUTF8 autoGateway(gateways[GET_FIELD(mLoopIndex)]);

  int type = getIpType(autoGateway.get());
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network route add %d %s %s/0 %s",
              GET_FIELD(mNetId), GET_CHAR(mIfname),
              type == AF_INET6 ? "::" : "0.0.0.0", autoGateway.get());

  struct MyCallback {
    static void callback(CommandCallback::CallbackType aOriginalCallback,
                         CommandChain* aChain,
                         bool aError,
                         mozilla::dom::NetworkResultOptions& aResult)
    {
      NS_ConvertUTF16toUTF8 reason(aResult.mResultReason);
      NU_DBG("addDefaultRouteToNetwork's reason: %s", reason.get());
      if (aError && !reason.EqualsASCII("addRoute() failed (File exists)")) {
        return aOriginalCallback(aChain, aError, aResult);
      }

      GET_FIELD(mLoopIndex)++;
      return addDefaultRouteToNetwork(aChain, aOriginalCallback, aResult);
    }
  };

  CommandCallback wrappedCallback(MyCallback::callback, aCallback);
  doCommand(command, aChain, wrappedCallback);
}

void NetworkUtils::setDefaultNetwork(CommandChain* aChain,
                                     CommandCallback aCallback,
                                     NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "network default set %d", GET_FIELD(mNetId));

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::addRouteToSecondaryTable(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult) {

  char command[MAX_COMMAND_SIZE];

  if (SDK_VERSION >= 20) {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1,
                "network route add %d %s %s/%s %s",
                GET_FIELD(mNetId),
                GET_CHAR(mIfname),
                GET_CHAR(mIp),
                GET_CHAR(mPrefix),
                GET_CHAR(mGateway));
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1,
                "interface route add %s secondary %s %s %s",
                GET_CHAR(mIfname),
                GET_CHAR(mIp),
                GET_CHAR(mPrefix),
                GET_CHAR(mGateway));
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::removeRouteFromSecondaryTable(CommandChain* aChain,
                                                 CommandCallback aCallback,
                                                 NetworkResultOptions& aResult) {
  char command[MAX_COMMAND_SIZE];

  if (SDK_VERSION >= 20) {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1,
                "network route remove %d %s %s/%s %s",
                GET_FIELD(mNetId),
                GET_CHAR(mIfname),
                GET_CHAR(mIp),
                GET_CHAR(mPrefix),
                GET_CHAR(mGateway));
  } else {
    PR_snprintf(command, MAX_COMMAND_SIZE - 1,
                "interface route remove %s secondary %s %s %s",
                GET_CHAR(mIfname),
                GET_CHAR(mIp),
                GET_CHAR(mPrefix),
                GET_CHAR(mGateway));
  }

  doCommand(command, aChain, aCallback);
}

void NetworkUtils::setIpv6Enabled(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult,
                                  bool aEnabled)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "interface ipv6 %s %s",
              GET_CHAR(mIfname), aEnabled ? "enable" : "disable");

  struct MyCallback {
    static void callback(CommandCallback::CallbackType aOriginalCallback,
                         CommandChain* aChain,
                         bool aError,
                         mozilla::dom::NetworkResultOptions& aResult)
    {
      aOriginalCallback(aChain, false, aResult);
    }
  };

  CommandCallback wrappedCallback(MyCallback::callback, aCallback);
  doCommand(command, aChain, wrappedCallback);
}

void NetworkUtils::enableIpv6(CommandChain* aChain,
                              CommandCallback aCallback,
                              NetworkResultOptions& aResult)
{
  setIpv6Enabled(aChain, aCallback, aResult, true);
}

void NetworkUtils::disableIpv6(CommandChain* aChain,
                               CommandCallback aCallback,
                               NetworkResultOptions& aResult)
{
  setIpv6Enabled(aChain, aCallback, aResult, false);
}

void NetworkUtils::setMtu(CommandChain* aChain,
                          CommandCallback aCallback,
                          NetworkResultOptions& aResult)
{
  char command[MAX_COMMAND_SIZE];
  PR_snprintf(command, MAX_COMMAND_SIZE - 1, "interface setmtu %s %d",
              GET_CHAR(mIfname), GET_FIELD(mMtu));

  doCommand(command, aChain, aCallback);
}

#undef GET_CHAR
#undef GET_FIELD

/*
 * Netd command success/fail function
 */
#define ASSIGN_FIELD(prop)  aResult.prop = aChain->getParams().prop;
#define ASSIGN_FIELD_VALUE(prop, value)  aResult.prop = value;

template<size_t N>
void NetworkUtils::runChain(const NetworkParams& aParams,
                            const CommandFunc (&aCmds)[N],
                            ErrorCallback aError)
{
  CommandChain* chain = new CommandChain(aParams, aCmds, N, aError);
  gCommandChainQueue.AppendElement(chain);

  if (gCommandChainQueue.Length() > 1) {
    NU_DBG("%d command chains are queued. Wait!", gCommandChainQueue.Length());
    return;
  }

  NetworkResultOptions result;
  NetworkUtils::next(gCommandChainQueue[0], false, result);
}

// Called to clean up the command chain and process the queued command chain if any.
void NetworkUtils::finalizeSuccess(CommandChain* aChain,
                                   NetworkResultOptions& aResult)
{
  next(aChain, false, aResult);
}

void NetworkUtils::wifiTetheringFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  // Notify the main thread.
  postMessage(aOptions, aResult);

  // If one of the stages fails, we try roll back to ensure
  // we don't leave the network systems in limbo.
  ASSIGN_FIELD_VALUE(mEnable, false)
  runChain(aOptions, sWifiFailChain, nullptr);
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
  finalizeSuccess(aChain, aResult);
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
    runChain(aOptions, sUSBFailChain, nullptr);
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
  finalizeSuccess(aChain, aResult);
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
  finalizeSuccess(aChain, aResult);
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
  finalizeSuccess(aChain, aResult);
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
  finalizeSuccess(aChain, aResult);
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
  finalizeSuccess(aChain, aResult);
}

void NetworkUtils::setDnsFail(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  postMessage(aOptions, aResult);
}

void NetworkUtils::defaultAsyncSuccessHandler(CommandChain* aChain,
                                              CommandCallback aCallback,
                                              NetworkResultOptions& aResult)
{
  NU_DBG("defaultAsyncSuccessHandler");
  aResult.mRet = true;
  postMessage(aChain->getParams(), aResult);
  finalizeSuccess(aChain, aResult);
}

void NetworkUtils::defaultAsyncFailureHandler(NetworkParams& aOptions, NetworkResultOptions& aResult)
{
  aResult.mRet = false;
  postMessage(aOptions, aResult);
}

#undef ASSIGN_FIELD
#undef ASSIGN_FIELD_VALUE

NetworkUtils::NetworkUtils(MessageCallback aCallback)
 : mMessageCallback(aCallback)
{
  mNetUtils = new NetUtils();

  char value[Property::VALUE_MAX_LENGTH];
  Property::Get("ro.build.version.sdk", value, nullptr);
  SDK_VERSION = atoi(value);

  Property::Get(IPV6_TETHERING, value, "0");
  SUPPORT_IPV6_TETHERING = atoi(value);

  gNetworkUtils = this;
}

NetworkUtils::~NetworkUtils()
{
}

#define GET_CHAR(prop) NS_ConvertUTF16toUTF8(aOptions.prop).get()
#define GET_FIELD(prop) aOptions.prop

// Hoist this type definition to global to avoid template
// instantiation error on gcc 4.4 used by ICS emulator.
typedef CommandResult (NetworkUtils::*CommandHandler)(NetworkParams&);
struct CommandHandlerEntry
{
  const char* mCommandName;
  CommandHandler mCommandHandler;
};

void NetworkUtils::ExecuteCommand(NetworkParams aOptions)
{
  const static CommandHandlerEntry
    COMMAND_HANDLER_TABLE[] = {

    // For command 'testCommand', BUILD_ENTRY(testCommand) will generate
    // {"testCommand", NetworkUtils::testCommand}
    #define BUILD_ENTRY(c) {#c, &NetworkUtils::c}

    BUILD_ENTRY(removeNetworkRoute),
    BUILD_ENTRY(setDNS),
    BUILD_ENTRY(setDefaultRoute),
    BUILD_ENTRY(removeDefaultRoute),
    BUILD_ENTRY(addHostRoute),
    BUILD_ENTRY(removeHostRoute),
    BUILD_ENTRY(addSecondaryRoute),
    BUILD_ENTRY(removeSecondaryRoute),
    BUILD_ENTRY(setNetworkInterfaceAlarm),
    BUILD_ENTRY(enableNetworkInterfaceAlarm),
    BUILD_ENTRY(disableNetworkInterfaceAlarm),
    BUILD_ENTRY(setTetheringAlarm),
    BUILD_ENTRY(removeTetheringAlarm),
    BUILD_ENTRY(getTetheringStatus),
    BUILD_ENTRY(setWifiOperationMode),
    BUILD_ENTRY(setDhcpServer),
    BUILD_ENTRY(setWifiTethering),
    BUILD_ENTRY(setUSBTethering),
    BUILD_ENTRY(enableUsbRndis),
    BUILD_ENTRY(updateUpStream),
    BUILD_ENTRY(configureInterface),
    BUILD_ENTRY(dhcpRequest),
    BUILD_ENTRY(stopDhcp),
    BUILD_ENTRY(enableInterface),
    BUILD_ENTRY(disableInterface),
    BUILD_ENTRY(resetConnections),
    BUILD_ENTRY(createNetwork),
    BUILD_ENTRY(destroyNetwork),
    BUILD_ENTRY(getNetId),
    BUILD_ENTRY(setMtu),

    #undef BUILD_ENTRY
  };

  // Loop until we find the command name which matches aOptions.mCmd.
  CommandHandler handler = nullptr;
  for (size_t i = 0; i < mozilla::ArrayLength(COMMAND_HANDLER_TABLE); i++) {
    if (aOptions.mCmd.EqualsASCII(COMMAND_HANDLER_TABLE[i].mCommandName)) {
      handler = COMMAND_HANDLER_TABLE[i].mCommandHandler;
      break;
    }
  }

  if (!handler) {
    // Command not found in COMMAND_HANDLER_TABLE.
    WARN("unknown message: %s", NS_ConvertUTF16toUTF8(aOptions.mCmd).get());
    return;
  }

  // The handler would return one of the following 3 values
  // to be wrapped to CommandResult:
  //
  //   1) |int32_t| for mostly synchronous native function calls.
  //   2) |NetworkResultOptions| to populate additional results. (e.g. dhcpRequest)
  //   3) |CommandResult::Pending| to indicate the result is not
  //      obtained yet.
  //
  // If the handler returns "Pending", the handler should take the
  // responsibility for posting result to main thread.
  CommandResult commandResult = (this->*handler)(aOptions);
  if (!commandResult.isPending()) {
    postMessage(aOptions, commandResult.mResult);
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
    NU_DBG("Receiving broadcast message from netd.");
    NU_DBG("          ==> Code: %d  Reason: %s", code, reason);
    sendBroadcastMessage(code, reason);

    if (code == NETD_COMMAND_INTERFACE_CHANGE) {
      if (gWifiTetheringParms) {
        char linkdownReason[MAX_COMMAND_SIZE];
        PR_snprintf(linkdownReason, MAX_COMMAND_SIZE - 1,
                    "Iface linkstate %s down",
                    NS_ConvertUTF16toUTF8(gWifiTetheringParms->mIfname).get());

        if (!strcmp(reason, linkdownReason)) {
          NU_DBG("Wifi link down, restarting tethering.");
          runChain(*gWifiTetheringParms, sWifiRetryChain, wifiTetheringFail);
        }
      }
    }

    nextNetdCommand();
    return;
  }

   // Set pending to false before we handle next command.
  NU_DBG("Receiving \"%s\" command response from netd.", gCurrentCommand.command);
  NU_DBG("          ==> Code: %d  Reason: %s", code, reason);

  gReason.AppendElement(nsCString(reason));

  // 1xx response code regards as command is proceeding, we need to wait for
  // final response code such as 2xx, 4xx and 5xx before sending next command.
  if (isProceeding(code)) {
    return;
  }

  if (isComplete(code)) {
    gPending = false;
  }

  {
    char buf[BUF_SIZE];
    join(gReason, INTERFACE_DELIMIT, BUF_SIZE, buf);

    NetworkResultOptions result;
    result.mResultCode = code;
    result.mResultReason = NS_ConvertUTF8toUTF16(buf);
    (gCurrentCommand.callback)(gCurrentCommand.chain, isError(code), result);
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
CommandResult NetworkUtils::setDhcpServer(NetworkParams& aOptions)
{
  if (aOptions.mEnabled) {
    aOptions.mWifiStartIp = aOptions.mStartIp;
    aOptions.mWifiEndIp = aOptions.mEndIp;
    aOptions.mIp = aOptions.mServerIp;
    aOptions.mPrefix = aOptions.mMaskLength;
    aOptions.mLink = NS_ConvertUTF8toUTF16("up");

    runChain(aOptions, sStartDhcpServerChain, setDhcpServerFail);
  } else {
    runChain(aOptions, sStopDhcpServerChain, setDhcpServerFail);
  }
  return CommandResult::Pending();
}

/**
 * Set DNS servers for given network interface.
 */
CommandResult NetworkUtils::setDNS(NetworkParams& aOptions)
{
  uint32_t length = aOptions.mDnses.Length();

  if (length > 0) {
    for (uint32_t i = 0; i < length; i++) {
      NS_ConvertUTF16toUTF8 autoDns(aOptions.mDnses[i]);

      char dns_prop_key[Property::VALUE_MAX_LENGTH];
      PR_snprintf(dns_prop_key, sizeof dns_prop_key, "net.dns%d", i+1);
      Property::Set(dns_prop_key, autoDns.get());
    }
  } else {
    // Set dnses from system properties.
    IFProperties interfaceProperties;
    getIFProperties(GET_CHAR(mIfname), interfaceProperties);

    Property::Set("net.dns1", interfaceProperties.dns1);
    Property::Set("net.dns2", interfaceProperties.dns2);
  }

  // Bump the DNS change property.
  char dnschange[Property::VALUE_MAX_LENGTH];
  Property::Get("net.dnschange", dnschange, "0");

  char num[Property::VALUE_MAX_LENGTH];
  PR_snprintf(num, Property::VALUE_MAX_LENGTH - 1, "%d", atoi(dnschange) + 1);
  Property::Set("net.dnschange", num);

  // DNS needs to be set through netd since JellyBean (4.3).
  if (SDK_VERSION >= 20) {
    // Lollipop.
    static CommandFunc COMMAND_CHAIN[] = {
      setInterfaceDns,
      addDefaultRouteToNetwork,
      defaultAsyncSuccessHandler
    };
    NetIdManager::NetIdInfo netIdInfo;
    if (!mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
      return -1;
    }
    aOptions.mNetId = netIdInfo.mNetId;
    runChain(aOptions, COMMAND_CHAIN, setDnsFail);
    return CommandResult::Pending();
  }
  if (SDK_VERSION >= 18) {
    // JB, KK.
    static CommandFunc COMMAND_CHAIN[] = {
    #if ANDROID_VERSION == 18
      // Since we don't use per-interface DNS lookup feature on JB,
      // we need to set the default DNS interface whenever setting the
      // DNS name server.
      setDefaultInterface,
    #endif
      setInterfaceDns,
      defaultAsyncSuccessHandler
    };
    runChain(aOptions, COMMAND_CHAIN, setDnsFail);
    return CommandResult::Pending();
  }

  return SUCCESS;
}

CommandResult NetworkUtils::configureInterface(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  return mNetUtils->do_ifc_configure(
    autoIfname.get(),
    aOptions.mIpaddr,
    aOptions.mMask,
    aOptions.mGateway_long,
    aOptions.mDns1_long,
    aOptions.mDns2_long
  );
}

CommandResult NetworkUtils::stopDhcp(NetworkParams& aOptions)
{
  return mNetUtils->do_dhcp_stop(GET_CHAR(mIfname));
}

CommandResult NetworkUtils::dhcpRequest(NetworkParams& aOptions) {
    mozilla::dom::NetworkResultOptions result;

    NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
    char ipaddr[Property::VALUE_MAX_LENGTH];
    char gateway[Property::VALUE_MAX_LENGTH];
    uint32_t prefixLength;
    char dns1[Property::VALUE_MAX_LENGTH];
    char dns2[Property::VALUE_MAX_LENGTH];
    char server[Property::VALUE_MAX_LENGTH];
    uint32_t lease;
    char vendorinfo[Property::VALUE_MAX_LENGTH];
    int32_t ret = mNetUtils->do_dhcp_do_request(autoIfname.get(),
                                                ipaddr,
                                                gateway,
                                                &prefixLength,
                                                dns1,
                                                dns2,
                                                server,
                                                &lease,
                                                vendorinfo);

    RETURN_IF_FAILED(ret);

    result.mIpaddr_str = NS_ConvertUTF8toUTF16(ipaddr);
    result.mGateway_str = NS_ConvertUTF8toUTF16(gateway);
    result.mDns1_str = NS_ConvertUTF8toUTF16(dns1);
    result.mDns2_str = NS_ConvertUTF8toUTF16(dns2);
    result.mServer_str = NS_ConvertUTF8toUTF16(server);
    result.mVendor_str = NS_ConvertUTF8toUTF16(vendorinfo);
    result.mLease = lease;
    result.mMask = makeMask(prefixLength);

    uint32_t inet4; // only support IPv4 for now.

#define INET_PTON(var, field)                                                 \
  PR_BEGIN_MACRO                                                              \
    inet_pton(AF_INET, var, &inet4);                                          \
    result.field = inet4;                                                    \
  PR_END_MACRO

    INET_PTON(ipaddr, mIpaddr);
    INET_PTON(gateway, mGateway);

    if (dns1[0] != '\0') {
      INET_PTON(dns1, mDns1);
    }

    if (dns2[0] != '\0') {
      INET_PTON(dns2, mDns2);
    }

    INET_PTON(server, mServer);

    char inet_str[64];
    if (inet_ntop(AF_INET, &result.mMask, inet_str, sizeof(inet_str))) {
      result.mMask_str = NS_ConvertUTF8toUTF16(inet_str);
    }

    return result;
}

CommandResult NetworkUtils::enableInterface(NetworkParams& aOptions) {
  return mNetUtils->do_ifc_enable(
    NS_ConvertUTF16toUTF8(aOptions.mIfname).get());
}

CommandResult NetworkUtils::disableInterface(NetworkParams& aOptions) {
  return mNetUtils->do_ifc_disable(
    NS_ConvertUTF16toUTF8(aOptions.mIfname).get());
}

CommandResult NetworkUtils::resetConnections(NetworkParams& aOptions) {
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  return mNetUtils->do_ifc_reset_connections(
    NS_ConvertUTF16toUTF8(aOptions.mIfname).get(),
    RESET_ALL_ADDRESSES);
}

/**
 * Set default route and DNS servers for given network interface.
 */
CommandResult NetworkUtils::setDefaultRoute(NetworkParams& aOptions)
{
  if (SDK_VERSION < 20) {
    return setDefaultRouteLegacy(aOptions);
  }

  static CommandFunc COMMAND_CHAIN[] = {
    addDefaultRouteToNetwork,
    setDefaultNetwork,
    defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface");
    return -1;
  }

  aOptions.mNetId = netIdInfo.mNetId;
  aOptions.mLoopIndex = 0;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult::Pending();
}

/**
 * Set default route and DNS servers for given network interface by obsoleted libnetutils.
 */
CommandResult NetworkUtils::setDefaultRouteLegacy(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);

  uint32_t length = aOptions.mGateways.Length();
  if (length > 0) {
    for (uint32_t i = 0; i < length; i++) {
      NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateways[i]);

      int type = getIpType(autoGateway.get());
      if (type != AF_INET && type != AF_INET6) {
        continue;
      }

      if (type == AF_INET6) {
        RETURN_IF_FAILED(mNetUtils->do_ifc_add_route(autoIfname.get(), "::", 0, autoGateway.get()));
      } else { /* type == AF_INET */
        RETURN_IF_FAILED(mNetUtils->do_ifc_set_default_route(autoIfname.get(), inet_addr(autoGateway.get())));
      }
    }
  } else {
    // Set default froute from system properties.
    char key[Property::KEY_MAX_LENGTH];
    char gateway[Property::KEY_MAX_LENGTH];

    PR_snprintf(key, sizeof key - 1, "net.%s.gw", autoIfname.get());
    Property::Get(key, gateway, "");

    int type = getIpType(gateway);
    if (type != AF_INET && type != AF_INET6) {
      return EAFNOSUPPORT;
    }

    if (type == AF_INET6) {
      RETURN_IF_FAILED(mNetUtils->do_ifc_add_route(autoIfname.get(), "::", 0, gateway));
    } else { /* type == AF_INET */
      RETURN_IF_FAILED(mNetUtils->do_ifc_set_default_route(autoIfname.get(), inet_addr(gateway)));
    }
  }

  // Set the default DNS interface.
  if (SDK_VERSION >= 18) {
    // For JB, KK only.
    static CommandFunc COMMAND_CHAIN[] = {
      setDefaultInterface,
      defaultAsyncSuccessHandler
    };
    runChain(aOptions, COMMAND_CHAIN, setDnsFail);
    return CommandResult::Pending();
  }

  return SUCCESS;
}

/**
 * Remove default route for given network interface.
 */
CommandResult NetworkUtils::removeDefaultRoute(NetworkParams& aOptions)
{
  NU_DBG("Calling NetworkUtils::removeDefaultRoute");

  if (SDK_VERSION < 20) {
    return removeDefaultRouteLegacy(aOptions);
  }

  static CommandFunc COMMAND_CHAIN[] = {
    removeDefaultRoute,
    defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return -1;
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId, GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  aOptions.mLoopIndex = 0;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult::Pending();
}

/**
 * Remove default route for given network interface by obsoleted libnetutils.
 */
CommandResult NetworkUtils::removeDefaultRouteLegacy(NetworkParams& aOptions)
{
  // Legacy libnetutils calls before Lollipop.
  uint32_t length = aOptions.mGateways.Length();
  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateways[i]);

    int type = getIpType(autoGateway.get());
    if (type != AF_INET && type != AF_INET6) {
      return EAFNOSUPPORT;
    }

    WARN_IF_FAILED(mNetUtils->do_ifc_remove_route(GET_CHAR(mIfname),
                                                  type == AF_INET ? "0.0.0.0" : "::",
                                                  0, autoGateway.get()));
  }

  return SUCCESS;
}

/**
 * Add host route for given network interface.
 */
CommandResult NetworkUtils::addHostRoute(NetworkParams& aOptions)
{
  if (SDK_VERSION < 20) {
    return addHostRouteLegacy(aOptions);
  }

  static CommandFunc COMMAND_CHAIN[] = {
    addRouteToInterface,
    defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return -1;
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId, GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult::Pending();
}

/**
 * Add host route for given network interface.
 */
CommandResult NetworkUtils::addHostRouteLegacy(NetworkParams& aOptions)
{
  if (aOptions.mGateway.IsEmpty()) {
    ERROR("addHostRouteLegacy does not support empty gateway.");
    return EINVAL;
  }

  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  NS_ConvertUTF16toUTF8 autoHostname(aOptions.mIp);
  NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateway);
  int type, prefix;

  type = getIpType(autoHostname.get());
  if (type != AF_INET && type != AF_INET6) {
    return EAFNOSUPPORT;
  }

  if (type != getIpType(autoGateway.get())) {
    return EINVAL;
  }

  prefix = type == AF_INET ? 32 : 128;
  return mNetUtils->do_ifc_add_route(autoIfname.get(), autoHostname.get(),
                                     prefix, autoGateway.get());
}

/**
 * Remove host route for given network interface.
 */
CommandResult NetworkUtils::removeHostRoute(NetworkParams& aOptions)
{
  if (SDK_VERSION < 20) {
    return removeHostRouteLegacy(aOptions);
  }

  static CommandFunc COMMAND_CHAIN[] = {
    removeRouteFromInterface,
    defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return -1;
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId, GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult::Pending();
}

/**
 * Remove host route for given network interface.
 */
CommandResult NetworkUtils::removeHostRouteLegacy(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  NS_ConvertUTF16toUTF8 autoHostname(aOptions.mIp);
  NS_ConvertUTF16toUTF8 autoGateway(aOptions.mGateway);
  int type, prefix;

  type = getIpType(autoHostname.get());
  if (type != AF_INET && type != AF_INET6) {
    return EAFNOSUPPORT;
  }

  if (type != getIpType(autoGateway.get())) {
    return EINVAL;
  }

  prefix = type == AF_INET ? 32 : 128;
  return mNetUtils->do_ifc_remove_route(autoIfname.get(), autoHostname.get(),
                                        prefix, autoGateway.get());
}

CommandResult NetworkUtils::removeNetworkRoute(NetworkParams& aOptions)
{
  if (SDK_VERSION < 20) {
    return removeNetworkRouteLegacy(aOptions);
  }

  static CommandFunc COMMAND_CHAIN[] = {
    clearAddrForInterface,
    defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("interface %s is not present in any network", GET_CHAR(mIfname));
    return -1;
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId, GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult::Pending();
}

nsCString NetworkUtils::getSubnetIp(const nsCString& aIp, int aPrefixLength)
{
  int type = getIpType(aIp.get());

  if (AF_INET6 == type) {
    struct in6_addr in6;
    if (inet_pton(AF_INET6, aIp.get(), &in6) != 1) {
      return nsCString();
    }

    uint32_t p, i, p1, mask;
    p = aPrefixLength;
    i = 0;
    while (i < 4) {
      p1 = p > 32 ? 32 : p;
      p -= p1;
      mask = p1 ? ~0x0 << (32 - p1) : 0;
      in6.s6_addr32[i++] &= htonl(mask);
    }

    char subnetStr[INET6_ADDRSTRLEN];
    if (!inet_ntop(AF_INET6, &in6, subnetStr, sizeof subnetStr)) {
      return nsCString();
    }

    return nsCString(subnetStr);
  }

  if (AF_INET == type) {
    uint32_t ip = inet_addr(aIp.get());
    uint32_t netmask = makeMask(aPrefixLength);
    uint32_t subnet = ip & netmask;
    struct in_addr addr;
    addr.s_addr = subnet;
    return nsCString(inet_ntoa(addr));
  }

  return nsCString();
}

CommandResult NetworkUtils::removeNetworkRouteLegacy(NetworkParams& aOptions)
{
  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  NS_ConvertUTF16toUTF8 autoIp(aOptions.mIp);

  int type = getIpType(autoIp.get());
  if (type != AF_INET && type != AF_INET6) {
    return EAFNOSUPPORT;
  }

  uint32_t prefixLength = GET_FIELD(mPrefixLength);

  if (type == AF_INET6) {
    // Calculate subnet.
    struct in6_addr in6;
    if (inet_pton(AF_INET6, autoIp.get(), &in6) != 1) {
      return EINVAL;
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
      return EINVAL;
    }

    // Remove default route.
    WARN_IF_FAILED(mNetUtils->do_ifc_remove_route(autoIfname.get(), "::", 0, NULL));

    // Remove subnet route.
    RETURN_IF_FAILED(mNetUtils->do_ifc_remove_route(autoIfname.get(), subnetStr, prefixLength, NULL));
    return SUCCESS;
  }

  /* type == AF_INET */
  uint32_t ip = inet_addr(autoIp.get());
  uint32_t netmask = makeMask(prefixLength);
  uint32_t subnet = ip & netmask;
  const char* gateway = "0.0.0.0";
  struct in_addr addr;
  addr.s_addr = subnet;
  const char* dst = inet_ntoa(addr);

  RETURN_IF_FAILED(mNetUtils->do_ifc_remove_default_route(autoIfname.get()));
  RETURN_IF_FAILED(mNetUtils->do_ifc_remove_route(autoIfname.get(), dst, prefixLength, gateway));
  return SUCCESS;
}

CommandResult NetworkUtils::addSecondaryRoute(NetworkParams& aOptions)
{
  static CommandFunc COMMAND_CHAIN[] = {
    addRouteToSecondaryTable,
    defaultAsyncSuccessHandler
  };

  if (SDK_VERSION >= 20) {
    NetIdManager::NetIdInfo netIdInfo;
    if (!mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
      return -1;
    }
    aOptions.mNetId = netIdInfo.mNetId;
  }

  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult::Pending();
}

CommandResult NetworkUtils::removeSecondaryRoute(NetworkParams& aOptions)
{
  static CommandFunc COMMAND_CHAIN[] = {
    removeRouteFromSecondaryTable,
    defaultAsyncSuccessHandler
  };

  if (SDK_VERSION >= 20) {
    NetIdManager::NetIdInfo netIdInfo;
    if (!mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
      return -1;
    }
    aOptions.mNetId = netIdInfo.mNetId;
  }

  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult::Pending();
}

CommandResult NetworkUtils::setNetworkInterfaceAlarm(NetworkParams& aOptions)
{
  NU_DBG("setNetworkInterfaceAlarms: %s", GET_CHAR(mIfname));
  runChain(aOptions, sNetworkInterfaceSetAlarmChain, networkInterfaceAlarmFail);
  return CommandResult::Pending();
}

CommandResult NetworkUtils::enableNetworkInterfaceAlarm(NetworkParams& aOptions)
{
  NU_DBG("enableNetworkInterfaceAlarm: %s", GET_CHAR(mIfname));
  runChain(aOptions, sNetworkInterfaceEnableAlarmChain, networkInterfaceAlarmFail);
  return CommandResult::Pending();
}

CommandResult NetworkUtils::disableNetworkInterfaceAlarm(NetworkParams& aOptions)
{
  NU_DBG("disableNetworkInterfaceAlarms: %s", GET_CHAR(mIfname));
  runChain(aOptions, sNetworkInterfaceDisableAlarmChain, networkInterfaceAlarmFail);
  return CommandResult::Pending();
}

CommandResult NetworkUtils::setTetheringAlarm(NetworkParams& aOptions)
{
  NU_DBG("setTetheringAlarm");
  runChain(aOptions, sTetheringInterfaceSetAlarmChain, networkInterfaceAlarmFail);
  return CommandResult::Pending();
}

CommandResult NetworkUtils::removeTetheringAlarm(NetworkParams& aOptions)
{
  NU_DBG("removeTetheringAlarm");
  runChain(aOptions, sTetheringInterfaceRemoveAlarmChain, networkInterfaceAlarmFail);
  return CommandResult::Pending();
}

CommandResult NetworkUtils::getTetheringStatus(NetworkParams& aOptions)
{
  NU_DBG("getTetheringStatus");
  runChain(aOptions, sTetheringGetStatusChain, networkInterfaceAlarmFail);
  return CommandResult::Pending();
}

/**
 * handling main thread's reload Wifi firmware request
 */
CommandResult NetworkUtils::setWifiOperationMode(NetworkParams& aOptions)
{
  NU_DBG("setWifiOperationMode: %s %s", GET_CHAR(mIfname), GET_CHAR(mMode));
  runChain(aOptions, sWifiOperationModeChain, wifiOperationModeFail);
  return CommandResult::Pending();
}

/**
 * handling main thread's enable/disable WiFi Tethering request
 */
CommandResult NetworkUtils::setWifiTethering(NetworkParams& aOptions)
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

  if (SDK_VERSION >= 20) {
    NetIdManager::NetIdInfo netIdInfo;
    if (!mNetIdManager.lookup(aOptions.mExternalIfname, &netIdInfo)) {
      ERROR("No such interface: %s", GET_CHAR(mExternalIfname));
      return -1;
    }
    aOptions.mNetId = netIdInfo.mNetId;
  }

  if (enable) {
    NU_DBG("Starting Wifi Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    runChain(aOptions, sWifiEnableChain, wifiTetheringFail);
  } else {
    NU_DBG("Stopping Wifi Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    runChain(aOptions, sWifiDisableChain, wifiTetheringFail);
  }
  return CommandResult::Pending();
}

CommandResult NetworkUtils::setUSBTethering(NetworkParams& aOptions)
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

  if (SDK_VERSION >= 20) {
    NetIdManager::NetIdInfo netIdInfo;
    if (!mNetIdManager.lookup(aOptions.mExternalIfname, &netIdInfo)) {
      ERROR("No such interface: %s", GET_CHAR(mExternalIfname));
      return -1;
    }
    aOptions.mNetId = netIdInfo.mNetId;
  }

  if (enable) {
    NU_DBG("Starting USB Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    runChain(aOptions, sUSBEnableChain, usbTetheringFail);
  } else {
    NU_DBG("Stopping USB Tethering on %s <-> %s",
           GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
    runChain(aOptions, sUSBDisableChain, usbTetheringFail);
  }
  return CommandResult::Pending();
}

void NetworkUtils::escapeQuote(nsCString& aString)
{
  aString.ReplaceSubstring("\\", "\\\\");
  aString.ReplaceSubstring("\"", "\\\"");
}

CommandResult NetworkUtils::checkUsbRndisState(NetworkParams& aOptions)
{
  static uint32_t retry = 0;

  char currentState[Property::VALUE_MAX_LENGTH];
  Property::Get(SYS_USB_STATE_PROPERTY, currentState, nullptr);

  nsTArray<nsCString> stateFuncs;
  split(currentState, USB_CONFIG_DELIMIT, stateFuncs);
  bool rndisPresent = stateFuncs.Contains(nsCString(USB_FUNCTION_RNDIS));

  if (aOptions.mEnable == rndisPresent) {
    NetworkResultOptions result;
    result.mEnable = aOptions.mEnable;
    result.mResult = true;
    retry = 0;
    return result;
  }
  if (retry < USB_FUNCTION_RETRY_TIMES) {
    retry++;
    usleep(USB_FUNCTION_RETRY_INTERVAL * 1000);
    return checkUsbRndisState(aOptions);
  }

  NetworkResultOptions result;
  result.mResult = false;
  retry = 0;
  return result;
}

/**
 * Modify usb function's property to turn on USB RNDIS function
 */
CommandResult NetworkUtils::enableUsbRndis(NetworkParams& aOptions)
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

  char currentConfig[Property::VALUE_MAX_LENGTH];
  Property::Get(SYS_USB_CONFIG_PROPERTY, currentConfig, nullptr);

  nsTArray<nsCString> configFuncs;
  split(currentConfig, USB_CONFIG_DELIMIT, configFuncs);

  char persistConfig[Property::VALUE_MAX_LENGTH];
  Property::Get(PERSIST_SYS_USB_CONFIG_PROPERTY, persistConfig, nullptr);

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

  char newConfig[Property::VALUE_MAX_LENGTH] = "";
  Property::Get(SYS_USB_CONFIG_PROPERTY, currentConfig, nullptr);
  join(configFuncs, USB_CONFIG_DELIMIT, Property::VALUE_MAX_LENGTH, newConfig);
  if (strcmp(currentConfig, newConfig)) {
    Property::Set(SYS_USB_CONFIG_PROPERTY, newConfig);
  }

  // Trigger the timer to check usb state and report the result to NetworkManager.
  if (report) {
    usleep(USB_FUNCTION_RETRY_INTERVAL * 1000);
    return checkUsbRndisState(aOptions);
  }
  return SUCCESS;
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::updateUpStream(NetworkParams& aOptions)
{
  runChain(aOptions, sUpdateUpStreamChain, updateUpStreamFail);
  return CommandResult::Pending();
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::createNetwork(NetworkParams& aOptions)
{
  if (SDK_VERSION < 20) {
    return SUCCESS;
  }

  static CommandFunc COMMAND_CHAIN[] = {
    createNetwork,
    enableIpv6,
    addInterfaceToNetwork,
    defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  mNetIdManager.acquire(GET_FIELD(mIfname), &netIdInfo);
  if (netIdInfo.mRefCnt > 1) {
    // Already created. Just return.
    NU_DBG("Interface %s (%d) has been created.", GET_CHAR(mIfname),
                                                  netIdInfo.mNetId);
    return SUCCESS;
  }

  NU_DBG("Request netd to create a network with netid %d", netIdInfo.mNetId);
  // Newly created netid. Ask netd to create network.
  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult::Pending();
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::destroyNetwork(NetworkParams& aOptions)
{
  if (SDK_VERSION < 20) {
    return SUCCESS;
  }

  static CommandFunc COMMAND_CHAIN[] = {
    disableIpv6,
    destroyNetwork,
    defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.release(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No existing netid for %s", GET_CHAR(mIfname));
    return -1;
  }

  if (netIdInfo.mRefCnt > 0) {
    // Still be referenced. Just return.
    NU_DBG("Someone is still using this interface.");
    return SUCCESS;
  }

  NU_DBG("Interface %s (%d) is no longer used. Tell netd to destroy.",
         GET_CHAR(mIfname), netIdInfo.mNetId);

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult::Pending();
}

/**
 * Query the netId associated with the given network interface name.
 */
CommandResult NetworkUtils::getNetId(NetworkParams& aOptions)
{
  NetworkResultOptions result;

  if (SDK_VERSION < 20) {
    // For pre-Lollipop, use the interface name as the fallback.
    result.mNetId = GET_FIELD(mIfname);
    return result;
  }

  NetIdManager::NetIdInfo netIdInfo;
  if (-1 == mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    return ESRCH;
  }
  result.mNetId.AppendInt(netIdInfo.mNetId, 10);
  return result;
}

CommandResult NetworkUtils::setMtu(NetworkParams& aOptions)
{
  // Setting/getting mtu is supported since Kitkat.
  if (SDK_VERSION < 19) {
    ERROR("setMtu is not supported in current SDK_VERSION.");
    return -1;
  }

  static CommandFunc COMMAND_CHAIN[] = {
    setMtu,
    defaultAsyncSuccessHandler,
  };

  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult::Pending();
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
  NU_DBG("Dump params:");
  NU_DBG("     ifname: %s", GET_CHAR(mIfname));
  NU_DBG("     ip: %s", GET_CHAR(mIp));
  NU_DBG("     link: %s", GET_CHAR(mLink));
  NU_DBG("     prefix: %s", GET_CHAR(mPrefix));
  NU_DBG("     wifiStartIp: %s", GET_CHAR(mWifiStartIp));
  NU_DBG("     wifiEndIp: %s", GET_CHAR(mWifiEndIp));
  NU_DBG("     usbStartIp: %s", GET_CHAR(mUsbStartIp));
  NU_DBG("     usbEndIp: %s", GET_CHAR(mUsbEndIp));
  NU_DBG("     dnsserver1: %s", GET_CHAR(mDns1));
  NU_DBG("     dnsserver2: %s", GET_CHAR(mDns2));
  NU_DBG("     internalIfname: %s", GET_CHAR(mInternalIfname));
  NU_DBG("     externalIfname: %s", GET_CHAR(mExternalIfname));
  if (!strcmp(aType, "WIFI")) {
    NU_DBG("     wifictrlinterfacename: %s", GET_CHAR(mWifictrlinterfacename));
    NU_DBG("     ssid: %s", GET_CHAR(mSsid));
    NU_DBG("     security: %s", GET_CHAR(mSecurity));
    NU_DBG("     key: %s", GET_CHAR(mKey));
  }
#endif
}

#undef GET_CHAR
#undef GET_FIELD
