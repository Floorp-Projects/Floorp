/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcMessageHandler_h
#define NfcMessageHandler_h

#include "nsString.h"
#include "nsTArray.h"

namespace android {
class MOZ_EXPORT Parcel;
} // namespace android

namespace mozilla {

class CommandOptions;
class EventOptions;

class NfcMessageHandler
{
public:
  bool Marshall(android::Parcel& aParcel, const CommandOptions& aOptions);
  bool Unmarshall(const android::Parcel& aParcel, EventOptions& aOptions);

private:
  bool GeneralResponse(const android::Parcel& aParcel, EventOptions& aOptions);
  bool ConfigRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool ConfigResponse(const android::Parcel& aParcel, EventOptions& aOptions);
  bool ReadNDEFRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool ReadNDEFResponse(const android::Parcel& aParcel, EventOptions& aOptions);
  bool WriteNDEFRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool MakeReadOnlyNDEFRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool ConnectRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool CloseRequest(android::Parcel& aParcel, const CommandOptions& options);

  bool InitializeNotification(const android::Parcel& aParcel, EventOptions& aOptions);
  bool TechDiscoveredNotification(const android::Parcel& aParcel, EventOptions& aOptions);
  bool TechLostNotification(const android::Parcel& aParcel, EventOptions& aOptions);
  bool HCIEventTransactionNotification(const android::Parcel& aParcel, EventOptions& aOptions);

  bool ReadNDEFMessage(const android::Parcel& aParcel, EventOptions& aOptions);
  bool WriteNDEFMessage(android::Parcel& aParcel, const CommandOptions& aOptions);

private:
  nsTArray<int32_t> mPendingReqQueue;
  nsTArray<nsString> mRequestIdQueue;
  nsTArray<int32_t> mPowerLevelQueue;
};

} // namespace mozilla

#endif // NfcMessageHandler_h
