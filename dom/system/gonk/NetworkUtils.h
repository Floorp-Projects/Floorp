/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkUtils_h
#define NetworkUtils_h

#include "nsString.h"
#include "mozilla/dom/NetworkOptionsBinding.h"
#include "mozilla/dom/network/NetUtils.h"
#include "mozilla/ipc/Netd.h"
#include "nsTArray.h"
#include "NetIdManager.h"

class NetworkParams;
class CommandChain;

class CommandCallback {
public:
  typedef void (*CallbackType)(CommandChain*, bool,
                               mozilla::dom::NetworkResultOptions& aResult);

  typedef void (*CallbackWrapperType)(CallbackType aOriginalCallback,
                                      CommandChain*, bool,
                                      mozilla::dom::NetworkResultOptions& aResult);

  CommandCallback()
    : mCallback(nullptr)
    , mCallbackWrapper(nullptr)
  {
  }

  CommandCallback(CallbackType aCallback)
    : mCallback(aCallback)
    , mCallbackWrapper(nullptr)
  {
  }

  CommandCallback(CallbackWrapperType aCallbackWrapper,
                  CommandCallback aOriginalCallback)
    : mCallback(aOriginalCallback.mCallback)
    , mCallbackWrapper(aCallbackWrapper)
  {
  }

  void operator()(CommandChain* aChain, bool aError,
                  mozilla::dom::NetworkResultOptions& aResult)
  {
    if (mCallbackWrapper) {
      return mCallbackWrapper(mCallback, aChain, aError, aResult);
    }
    if (mCallback) {
      return mCallback(aChain, aError, aResult);
    }
  }

private:
  CallbackType mCallback;
  CallbackWrapperType mCallbackWrapper;
};

typedef void (*CommandFunc)(CommandChain*, CommandCallback,
                            mozilla::dom::NetworkResultOptions& aResult);
typedef void (*MessageCallback)(mozilla::dom::NetworkResultOptions& aResult);
typedef void (*ErrorCallback)(NetworkParams& aOptions,
                              mozilla::dom::NetworkResultOptions& aResult);

class NetworkParams
{
public:
  NetworkParams() {
  }

  NetworkParams(const mozilla::dom::NetworkCommandOptions& aOther) {

#define COPY_SEQUENCE_FIELD(prop, type)                                                      \
    if (aOther.prop.WasPassed()) {                                                           \
      mozilla::dom::Sequence<type > const & currentValue = aOther.prop.InternalValue();      \
      uint32_t length = currentValue.Length();                                               \
      for (uint32_t idx = 0; idx < length; idx++) {                                          \
        prop.AppendElement(currentValue[idx]);                                               \
      }                                                                                      \
    }

#define COPY_OPT_STRING_FIELD(prop, defaultValue)       \
    if (aOther.prop.WasPassed()) {                      \
      if (aOther.prop.Value().EqualsLiteral("null")) {  \
        prop = defaultValue;                            \
      } else {                                          \
        prop = aOther.prop.Value();                     \
      }                                                 \
    } else {                                            \
      prop = defaultValue;                              \
    }

#define COPY_OPT_FIELD(prop, defaultValue)            \
    if (aOther.prop.WasPassed()) {                    \
      prop = aOther.prop.Value();                     \
    } else {                                          \
      prop = defaultValue;                            \
    }

#define COPY_FIELD(prop) prop = aOther.prop;

    COPY_FIELD(mId)
    COPY_FIELD(mCmd)
    COPY_OPT_STRING_FIELD(mDomain, EmptyString())
    COPY_OPT_STRING_FIELD(mGateway, EmptyString())
    COPY_SEQUENCE_FIELD(mGateways, nsString)
    COPY_OPT_STRING_FIELD(mIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mIp, EmptyString())
    COPY_OPT_FIELD(mPrefixLength, 0)
    COPY_OPT_STRING_FIELD(mMode, EmptyString())
    COPY_OPT_FIELD(mReport, false)
    COPY_OPT_FIELD(mEnabled, false)
    COPY_OPT_STRING_FIELD(mWifictrlinterfacename, EmptyString())
    COPY_OPT_STRING_FIELD(mInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mExternalIfname, EmptyString())
    COPY_OPT_FIELD(mEnable, false)
    COPY_OPT_STRING_FIELD(mSsid, EmptyString())
    COPY_OPT_STRING_FIELD(mSecurity, EmptyString())
    COPY_OPT_STRING_FIELD(mKey, EmptyString())
    COPY_OPT_STRING_FIELD(mPrefix, EmptyString())
    COPY_OPT_STRING_FIELD(mLink, EmptyString())
    COPY_SEQUENCE_FIELD(mInterfaceList, nsString)
    COPY_OPT_STRING_FIELD(mWifiStartIp, EmptyString())
    COPY_OPT_STRING_FIELD(mWifiEndIp, EmptyString())
    COPY_OPT_STRING_FIELD(mUsbStartIp, EmptyString())
    COPY_OPT_STRING_FIELD(mUsbEndIp, EmptyString())
    COPY_OPT_STRING_FIELD(mDns1, EmptyString())
    COPY_OPT_STRING_FIELD(mDns2, EmptyString())
    COPY_SEQUENCE_FIELD(mDnses, nsString)
    COPY_OPT_STRING_FIELD(mStartIp, EmptyString())
    COPY_OPT_STRING_FIELD(mEndIp, EmptyString())
    COPY_OPT_STRING_FIELD(mServerIp, EmptyString())
    COPY_OPT_STRING_FIELD(mMaskLength, EmptyString())
    COPY_OPT_STRING_FIELD(mPreInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mPreExternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mCurInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mCurExternalIfname, EmptyString())
    COPY_OPT_FIELD(mThreshold, -1)
    COPY_OPT_FIELD(mIpaddr, 0)
    COPY_OPT_FIELD(mMask, 0)
    COPY_OPT_FIELD(mGateway_long, 0)
    COPY_OPT_FIELD(mDns1_long, 0)
    COPY_OPT_FIELD(mDns2_long, 0)
    COPY_OPT_FIELD(mMtu, 0)

    mLoopIndex = 0;

#undef COPY_SEQUENCE_FIELD
#undef COPY_OPT_STRING_FIELD
#undef COPY_OPT_FIELD
#undef COPY_FIELD
  }

  // Followings attributes are 1-to-1 mapping to NetworkCommandOptions.
  int32_t mId;
  nsString mCmd;
  nsString mDomain;
  nsString mGateway;
  nsTArray<nsString> mGateways;
  nsString mIfname;
  nsString mIp;
  uint32_t mPrefixLength;
  nsString mMode;
  bool mReport;
  bool mEnabled;
  nsString mWifictrlinterfacename;
  nsString mInternalIfname;
  nsString mExternalIfname;
  bool mEnable;
  nsString mSsid;
  nsString mSecurity;
  nsString mKey;
  nsString mPrefix;
  nsString mLink;
  nsTArray<nsString> mInterfaceList;
  nsString mWifiStartIp;
  nsString mWifiEndIp;
  nsString mUsbStartIp;
  nsString mUsbEndIp;
  nsString mDns1;
  nsString mDns2;
  nsTArray<nsString> mDnses;
  nsString mStartIp;
  nsString mEndIp;
  nsString mServerIp;
  nsString mMaskLength;
  nsString mPreInternalIfname;
  nsString mPreExternalIfname;
  nsString mCurInternalIfname;
  nsString mCurExternalIfname;
  long mThreshold;
  long mIpaddr;
  long mMask;
  long mGateway_long;
  long mDns1_long;
  long mDns2_long;
  long mMtu;

  // Auxiliary information required to carry accros command chain.
  int mNetId; // A locally defined id per interface.
  uint32_t mLoopIndex; // Loop index for adding/removing multiple gateways.
};

// CommandChain store the necessary information to execute command one by one.
// Including :
// 1. Command parameters.
// 2. Command list.
// 3. Error callback function.
// 4. Index of current execution command.
class CommandChain final
{
public:
  CommandChain(const NetworkParams& aParams,
               const CommandFunc aCmds[],
               uint32_t aLength,
               ErrorCallback aError)
  : mIndex(-1)
  , mParams(aParams)
  , mCommands(aCmds)
  , mLength(aLength)
  , mError(aError) {
  }

  NetworkParams&
  getParams()
  {
    return mParams;
  };

  CommandFunc
  getNextCommand()
  {
    mIndex++;
    return mIndex < mLength ? mCommands[mIndex] : nullptr;
  };

  ErrorCallback
  getErrorCallback() const
  {
    return mError;
  };

private:
  uint32_t mIndex;
  NetworkParams mParams;
  const CommandFunc* mCommands;
  uint32_t mLength;
  ErrorCallback mError;
};

// A helper class to easily construct a resolved
// or a pending result for command execution.
class CommandResult
{
public:
  struct Pending {};

public:
  CommandResult(int32_t aResultCode);
  CommandResult(const mozilla::dom::NetworkResultOptions& aResult);
  CommandResult(const Pending&);
  bool isPending() const;

  mozilla::dom::NetworkResultOptions mResult;

private:
  bool mIsPending;
};

class NetworkUtils final
{
public:
  NetworkUtils(MessageCallback aCallback);
  ~NetworkUtils();

  void ExecuteCommand(NetworkParams aOptions);
  void onNetdMessage(mozilla::ipc::NetdCommand* aCommand);

  MessageCallback getMessageCallback() { return mMessageCallback; }

private:
  /**
   * Commands supported by NetworkUtils.
   */
  CommandResult configureInterface(NetworkParams& aOptions);
  CommandResult dhcpRequest(NetworkParams& aOptions);
  CommandResult stopDhcp(NetworkParams& aOptions);
  CommandResult enableInterface(NetworkParams& aOptions);
  CommandResult disableInterface(NetworkParams& aOptions);
  CommandResult resetConnections(NetworkParams& aOptions);
  CommandResult setDefaultRoute(NetworkParams& aOptions);
  CommandResult addHostRoute(NetworkParams& aOptions);
  CommandResult removeDefaultRoute(NetworkParams& aOptions);
  CommandResult removeHostRoute(NetworkParams& aOptions);
  CommandResult removeNetworkRoute(NetworkParams& aOptions);
  CommandResult setDNS(NetworkParams& aOptions);
  CommandResult addSecondaryRoute(NetworkParams& aOptions);
  CommandResult removeSecondaryRoute(NetworkParams& aOptions);
  CommandResult setNetworkInterfaceAlarm(NetworkParams& aOptions);
  CommandResult enableNetworkInterfaceAlarm(NetworkParams& aOptions);
  CommandResult disableNetworkInterfaceAlarm(NetworkParams& aOptions);
  CommandResult setTetheringAlarm(NetworkParams& aOptions);
  CommandResult removeTetheringAlarm(NetworkParams& aOptions);
  CommandResult getTetheringStatus(NetworkParams& aOptions);
  CommandResult setWifiOperationMode(NetworkParams& aOptions);
  CommandResult setDhcpServer(NetworkParams& aOptions);
  CommandResult setWifiTethering(NetworkParams& aOptions);
  CommandResult setUSBTethering(NetworkParams& aOptions);
  CommandResult enableUsbRndis(NetworkParams& aOptions);
  CommandResult updateUpStream(NetworkParams& aOptions);
  CommandResult createNetwork(NetworkParams& aOptions);
  CommandResult destroyNetwork(NetworkParams& aOptions);
  CommandResult getNetId(NetworkParams& aOptions);
  CommandResult setMtu(NetworkParams& aOptions);

  CommandResult addHostRouteLegacy(NetworkParams& aOptions);
  CommandResult removeHostRouteLegacy(NetworkParams& aOptions);
  CommandResult setDefaultRouteLegacy(NetworkParams& aOptions);
  CommandResult removeDefaultRouteLegacy(NetworkParams& aOptions);
  CommandResult removeNetworkRouteLegacy(NetworkParams& aOptions);


  /**
   * function pointer array holds all netd commands should be executed
   * in sequence to accomplish a given command by other module.
   */
  static const CommandFunc sWifiEnableChain[];
  static const CommandFunc sWifiDisableChain[];
  static const CommandFunc sWifiFailChain[];
  static const CommandFunc sWifiRetryChain[];
  static const CommandFunc sWifiOperationModeChain[];
  static const CommandFunc sUSBEnableChain[];
  static const CommandFunc sUSBDisableChain[];
  static const CommandFunc sUSBFailChain[];
  static const CommandFunc sUpdateUpStreamChain[];
  static const CommandFunc sStartDhcpServerChain[];
  static const CommandFunc sStopDhcpServerChain[];
  static const CommandFunc sNetworkInterfaceEnableAlarmChain[];
  static const CommandFunc sNetworkInterfaceDisableAlarmChain[];
  static const CommandFunc sNetworkInterfaceSetAlarmChain[];
  static const CommandFunc sTetheringInterfaceSetAlarmChain[];
  static const CommandFunc sTetheringInterfaceRemoveAlarmChain[];
  static const CommandFunc sTetheringGetStatusChain[];
  /**
   * Individual netd command stored in command chain.
   */
#define PARAMS CommandChain* aChain, CommandCallback aCallback, \
               mozilla::dom::NetworkResultOptions& aResult
  static void wifiFirmwareReload(PARAMS);
  static void startAccessPointDriver(PARAMS);
  static void stopAccessPointDriver(PARAMS);
  static void setAccessPoint(PARAMS);
  static void cleanUpStream(PARAMS);
  static void createUpStream(PARAMS);
  static void startSoftAP(PARAMS);
  static void stopSoftAP(PARAMS);
  static void clearWifiTetherParms(PARAMS);
  static void enableAlarm(PARAMS);
  static void disableAlarm(PARAMS);
  static void setQuota(PARAMS);
  static void removeQuota(PARAMS);
  static void setAlarm(PARAMS);
  static void removeAlarm(PARAMS);
  static void setGlobalAlarm(PARAMS);
  static void removeGlobalAlarm(PARAMS);
  static void setInterfaceUp(PARAMS);
  static void tetherInterface(PARAMS);
  static void addInterfaceToLocalNetwork(PARAMS);
  static void addRouteToLocalNetwork(PARAMS);
  static void preTetherInterfaceList(PARAMS);
  static void postTetherInterfaceList(PARAMS);
  static void addUpstreamInterface(PARAMS);
  static void removeUpstreamInterface(PARAMS);
  static void setIpForwardingEnabled(PARAMS);
  static void tetheringStatus(PARAMS);
  static void stopTethering(PARAMS);
  static void startTethering(PARAMS);
  static void untetherInterface(PARAMS);
  static void removeInterfaceFromLocalNetwork(PARAMS);
  static void setDnsForwarders(PARAMS);
  static void enableNat(PARAMS);
  static void disableNat(PARAMS);
  static void setDefaultInterface(PARAMS);
  static void setInterfaceDns(PARAMS);
  static void wifiTetheringSuccess(PARAMS);
  static void usbTetheringSuccess(PARAMS);
  static void networkInterfaceAlarmSuccess(PARAMS);
  static void updateUpStreamSuccess(PARAMS);
  static void setDhcpServerSuccess(PARAMS);
  static void wifiOperationModeSuccess(PARAMS);
  static void clearAddrForInterface(PARAMS);
  static void createNetwork(PARAMS);
  static void destroyNetwork(PARAMS);
  static void addInterfaceToNetwork(PARAMS);
  static void addDefaultRouteToNetwork(PARAMS);
  static void setDefaultNetwork(PARAMS);
  static void removeDefaultRoute(PARAMS);
  static void removeNetworkRouteSuccess(PARAMS);
  static void removeNetworkRoute(PARAMS);
  static void addRouteToInterface(PARAMS);
  static void removeRouteFromInterface(PARAMS);
  static void modifyRouteOnInterface(PARAMS, bool aDoAdd);
  static void enableIpv6(PARAMS);
  static void disableIpv6(PARAMS);
  static void setMtu(PARAMS);
  static void setIpv6Enabled(PARAMS, bool aEnabled);
  static void addRouteToSecondaryTable(PARAMS);
  static void removeRouteFromSecondaryTable(PARAMS);
  static void defaultAsyncSuccessHandler(PARAMS);

#undef PARAMS

  /**
   * Error callback function executed when a command is fail.
   */
#define PARAMS NetworkParams& aOptions, \
               mozilla::dom::NetworkResultOptions& aResult
  static void wifiTetheringFail(PARAMS);
  static void wifiOperationModeFail(PARAMS);
  static void usbTetheringFail(PARAMS);
  static void updateUpStreamFail(PARAMS);
  static void setDhcpServerFail(PARAMS);
  static void networkInterfaceAlarmFail(PARAMS);
  static void setDnsFail(PARAMS);
  static void defaultAsyncFailureHandler(PARAMS);
#undef PARAMS

  /**
   * Command chain processing functions.
   */
  static void next(CommandChain* aChain, bool aError,
                   mozilla::dom::NetworkResultOptions& aResult);
  static void nextNetdCommand();
  static void doCommand(const char* aCommand, CommandChain* aChain, CommandCallback aCallback);

  /**
   * Notify broadcast message to main thread.
   */
  void sendBroadcastMessage(uint32_t code, char* reason);

  /**
   * Utility functions.
   */
  CommandResult checkUsbRndisState(NetworkParams& aOptions);
  void dumpParams(NetworkParams& aOptions, const char* aType);

  static void escapeQuote(nsCString& aString);
  inline uint32_t netdResponseType(uint32_t code);
  inline bool isBroadcastMessage(uint32_t code);
  inline bool isError(uint32_t code);
  inline bool isComplete(uint32_t code);
  inline bool isProceeding(uint32_t code);
  void Shutdown();
  static void runNextQueuedCommandChain();
  static void finalizeSuccess(CommandChain* aChain,
                              mozilla::dom::NetworkResultOptions& aResult);

  template<size_t N>
  static void runChain(const NetworkParams& aParams,
                       const CommandFunc (&aCmds)[N],
                       ErrorCallback aError);

  static nsCString getSubnetIp(const nsCString& aIp, int aPrefixLength);

  /**
   * Callback function to send netd result to main thread.
   */
  MessageCallback mMessageCallback;

  /*
   * Utility class to access libnetutils.
   */
  nsAutoPtr<NetUtils> mNetUtils;

  NetIdManager mNetIdManager;
};

#endif
