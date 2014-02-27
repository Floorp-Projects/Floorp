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

class NetworkParams;
class CommandChain;

using namespace mozilla::dom;

typedef void (*CommandCallback)(CommandChain*, bool, NetworkResultOptions& aResult);
typedef void (*CommandFunc)(CommandChain*, CommandCallback, NetworkResultOptions& aResult);
typedef void (*MessageCallback)(NetworkResultOptions& aResult);
typedef void (*ErrorCallback)(NetworkParams& aOptions, NetworkResultOptions& aResult);

class NetworkParams
{
public:
  NetworkParams() {
  }

  NetworkParams(const NetworkParams& aOther) {
    mIp = aOther.mIp;
    mCmd = aOther.mCmd;
    mDomain = aOther.mDomain;
    mDns1_str = aOther.mDns1_str;
    mDns2_str = aOther.mDns2_str;
    mGateway = aOther.mGateway;
    mGateway_str = aOther.mGateway_str;
    mHostnames = aOther.mHostnames;
    mId = aOther.mId;
    mIfname = aOther.mIfname;
    mPrefixLength = aOther.mPrefixLength;
    mOldIfname = aOther.mOldIfname;
    mMode = aOther.mMode;
    mReport = aOther.mReport;
    mIsAsync = aOther.mIsAsync;
    mEnabled = aOther.mEnabled;
    mWifictrlinterfacename = aOther.mWifictrlinterfacename;
    mInternalIfname = aOther.mInternalIfname;
    mExternalIfname = aOther.mExternalIfname;
    mEnable = aOther.mEnable;
    mSsid = aOther.mSsid;
    mSecurity = aOther.mSecurity;
    mKey = aOther.mKey;
    mPrefix = aOther.mPrefix;
    mLink = aOther.mLink;
    mInterfaceList = aOther.mInterfaceList;
    mWifiStartIp = aOther.mWifiStartIp;
    mWifiEndIp = aOther.mWifiEndIp;
    mUsbStartIp = aOther.mUsbStartIp;
    mUsbEndIp = aOther.mUsbEndIp;
    mDns1 = aOther.mDns1;
    mDns2 = aOther.mDns2;
    mRxBytes = aOther.mRxBytes;
    mTxBytes = aOther.mTxBytes;
    mDate = aOther.mDate;
    mStartIp = aOther.mStartIp;
    mEndIp = aOther.mEndIp;
    mServerIp = aOther.mServerIp;
    mMaskLength = aOther.mMaskLength;
    mPreInternalIfname = aOther.mPreInternalIfname;
    mPreExternalIfname = aOther.mPreExternalIfname;
    mCurInternalIfname = aOther.mCurInternalIfname;
    mCurExternalIfname = aOther.mCurExternalIfname;
    mThreshold = aOther.mThreshold;
  }

  NetworkParams(const NetworkCommandOptions& aOther) {

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
    COPY_OPT_STRING_FIELD(mDns1_str, EmptyString())
    COPY_OPT_STRING_FIELD(mDns2_str, EmptyString())
    COPY_OPT_STRING_FIELD(mGateway, EmptyString())
    COPY_OPT_STRING_FIELD(mGateway_str, EmptyString())
    COPY_SEQUENCE_FIELD(mHostnames, nsString)
    COPY_OPT_STRING_FIELD(mIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mIp, EmptyString())
    COPY_OPT_FIELD(mPrefixLength, 0)
    COPY_OPT_STRING_FIELD(mOldIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mMode, EmptyString())
    COPY_OPT_FIELD(mReport, false)
    COPY_OPT_FIELD(mIsAsync, true)
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
    COPY_OPT_FIELD(mRxBytes, -1)
    COPY_OPT_FIELD(mTxBytes, -1)
    COPY_OPT_STRING_FIELD(mDate, EmptyString())
    COPY_OPT_STRING_FIELD(mStartIp, EmptyString())
    COPY_OPT_STRING_FIELD(mEndIp, EmptyString())
    COPY_OPT_STRING_FIELD(mServerIp, EmptyString())
    COPY_OPT_STRING_FIELD(mMaskLength, EmptyString())
    COPY_OPT_STRING_FIELD(mPreInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mPreExternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mCurInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mCurExternalIfname, EmptyString())
    COPY_OPT_FIELD(mThreshold, -1)

#undef COPY_SEQUENCE_FIELD
#undef COPY_OPT_STRING_FIELD
#undef COPY_OPT_FIELD
#undef COPY_FIELD
  }

  int32_t mId;
  nsString mCmd;
  nsString mDomain;
  nsString mDns1_str;
  nsString mDns2_str;
  nsString mGateway;
  nsString mGateway_str;
  nsTArray<nsString> mHostnames;
  nsString mIfname;
  nsString mIp;
  uint32_t mPrefixLength;
  nsString mOldIfname;
  nsString mMode;
  bool mReport;
  bool mIsAsync;
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
  float mRxBytes;
  float mTxBytes;
  nsString mDate;
  nsString mStartIp;
  nsString mEndIp;
  nsString mServerIp;
  nsString mMaskLength;
  nsString mPreInternalIfname;
  nsString mPreExternalIfname;
  nsString mCurInternalIfname;
  nsString mCurExternalIfname;
  long mThreshold;
};

// CommandChain store the necessary information to execute command one by one.
// Including :
// 1. Command parameters.
// 2. Command list.
// 3. Error callback function.
// 4. Index of current execution command.
class CommandChain MOZ_FINAL
{
public:
  CommandChain(const NetworkParams& aParams,
               CommandFunc aCmds[],
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
  CommandFunc* mCommands;
  uint32_t mLength;
  ErrorCallback mError;
};

class NetworkUtils MOZ_FINAL
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
  bool setDNS(NetworkParams& aOptions);
  bool setDefaultRouteAndDNS(NetworkParams& aOptions);
  bool addHostRoute(NetworkParams& aOptions);
  bool removeDefaultRoute(NetworkParams& aOptions);
  bool removeHostRoute(NetworkParams& aOptions);
  bool removeHostRoutes(NetworkParams& aOptions);
  bool removeNetworkRoute(NetworkParams& aOptions);
  bool addSecondaryRoute(NetworkParams& aOptions);
  bool removeSecondaryRoute(NetworkParams& aOptions);
  bool getNetworkInterfaceStats(NetworkParams& aOptions);
  bool setNetworkInterfaceAlarm(NetworkParams& aOptions);
  bool enableNetworkInterfaceAlarm(NetworkParams& aOptions);
  bool disableNetworkInterfaceAlarm(NetworkParams& aOptions);
  bool setWifiOperationMode(NetworkParams& aOptions);
  bool setDhcpServer(NetworkParams& aOptions);
  bool setWifiTethering(NetworkParams& aOptions);
  bool setUSBTethering(NetworkParams& aOptions);
  bool enableUsbRndis(NetworkParams& aOptions);
  bool updateUpStream(NetworkParams& aOptions);

  /**
   * function pointer array holds all netd commands should be executed
   * in sequence to accomplish a given command by other module.
   */
  static CommandFunc sWifiEnableChain[];
  static CommandFunc sWifiDisableChain[];
  static CommandFunc sWifiFailChain[];
  static CommandFunc sWifiOperationModeChain[];
  static CommandFunc sUSBEnableChain[];
  static CommandFunc sUSBDisableChain[];
  static CommandFunc sUSBFailChain[];
  static CommandFunc sUpdateUpStreamChain[];
  static CommandFunc sStartDhcpServerChain[];
  static CommandFunc sStopDhcpServerChain[];
  static CommandFunc sNetworkInterfaceStatsChain[];
  static CommandFunc sNetworkInterfaceEnableAlarmChain[];
  static CommandFunc sNetworkInterfaceDisableAlarmChain[];
  static CommandFunc sNetworkInterfaceSetAlarmChain[];
  static CommandFunc sSetDnsChain[];

  /**
   * Individual netd command stored in command chain.
   */
#define PARAMS CommandChain* aChain, CommandCallback aCallback, NetworkResultOptions& aResult
  static void wifiFirmwareReload(PARAMS);
  static void startAccessPointDriver(PARAMS);
  static void stopAccessPointDriver(PARAMS);
  static void setAccessPoint(PARAMS);
  static void cleanUpStream(PARAMS);
  static void createUpStream(PARAMS);
  static void startSoftAP(PARAMS);
  static void stopSoftAP(PARAMS);
  static void getRxBytes(PARAMS);
  static void getTxBytes(PARAMS);
  static void enableAlarm(PARAMS);
  static void disableAlarm(PARAMS);
  static void setQuota(PARAMS);
  static void removeQuota(PARAMS);
  static void setAlarm(PARAMS);
  static void setInterfaceUp(PARAMS);
  static void tetherInterface(PARAMS);
  static void preTetherInterfaceList(PARAMS);
  static void postTetherInterfaceList(PARAMS);
  static void setIpForwardingEnabled(PARAMS);
  static void tetheringStatus(PARAMS);
  static void stopTethering(PARAMS);
  static void startTethering(PARAMS);
  static void untetherInterface(PARAMS);
  static void setDnsForwarders(PARAMS);
  static void enableNat(PARAMS);
  static void disableNat(PARAMS);
  static void setDefaultInterface(PARAMS);
  static void setInterfaceDns(PARAMS);
  static void wifiTetheringSuccess(PARAMS);
  static void usbTetheringSuccess(PARAMS);
  static void networkInterfaceStatsSuccess(PARAMS);
  static void networkInterfaceAlarmSuccess(PARAMS);
  static void updateUpStreamSuccess(PARAMS);
  static void setDhcpServerSuccess(PARAMS);
  static void wifiOperationModeSuccess(PARAMS);
#undef PARAMS

  /**
   * Error callback function executed when a command is fail.
   */
#define PARAMS NetworkParams& aOptions, NetworkResultOptions& aResult
  static void wifiTetheringFail(PARAMS);
  static void wifiOperationModeFail(PARAMS);
  static void usbTetheringFail(PARAMS);
  static void updateUpStreamFail(PARAMS);
  static void setDhcpServerFail(PARAMS);
  static void networkInterfaceStatsFail(PARAMS);
  static void networkInterfaceAlarmFail(PARAMS);
  static void setDnsFail(PARAMS);
#undef PARAMS

  /**
   * Command chain processing functions.
   */
  static void next(CommandChain* aChain, bool aError, NetworkResultOptions& aResult);
  static void nextNetdCommand();
  static void doCommand(const char* aCommand, CommandChain* aChain, CommandCallback aCallback);

  /**
   * Notify broadcast message to main thread.
   */
  void sendBroadcastMessage(uint32_t code, char* reason);

  /**
   * Utility functions.
   */
  void checkUsbRndisState(NetworkParams& aOptions);
  void dumpParams(NetworkParams& aOptions, const char* aType);

  inline uint32_t netdResponseType(uint32_t code);
  inline bool isBroadcastMessage(uint32_t code);
  inline bool isError(uint32_t code);
  inline bool isComplete(uint32_t code);
  inline bool isProceeding(uint32_t code);
  void Shutdown();
  /**
   * Callback function to send netd result to main thread.
   */
  MessageCallback mMessageCallback;

  /*
   * Utility class to access libnetutils.
   */
  nsAutoPtr<NetUtils> mNetUtils;
};

#endif
