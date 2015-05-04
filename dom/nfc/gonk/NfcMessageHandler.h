/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  bool ProcessResponse(int32_t aType, const android::Parcel& aParcel, EventOptions& aOptions);
  bool GeneralResponse(const android::Parcel& aParcel, EventOptions& aOptions);
  bool ChangeRFStateRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool ChangeRFStateResponse(const android::Parcel& aParcel, EventOptions& aOptions);
  bool ReadNDEFRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool ReadNDEFResponse(const android::Parcel& aParcel, EventOptions& aOptions);
  bool WriteNDEFRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool MakeReadOnlyRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool FormatRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool TransceiveRequest(android::Parcel& aParcel, const CommandOptions& options);
  bool TransceiveResponse(const android::Parcel& aParcel, EventOptions& aOptions);

  bool ProcessNotification(int32_t aType, const android::Parcel& aParcel, EventOptions& aOptions);
  bool InitializeNotification(const android::Parcel& aParcel, EventOptions& aOptions);
  bool TechDiscoveredNotification(const android::Parcel& aParcel, EventOptions& aOptions);
  bool TechLostNotification(const android::Parcel& aParcel, EventOptions& aOptions);
  bool HCIEventTransactionNotification(const android::Parcel& aParcel, EventOptions& aOptions);
  bool NDEFReceivedNotification(const android::Parcel& aParcel, EventOptions& aOptions);

  bool ReadNDEFMessage(const android::Parcel& aParcel, EventOptions& aOptions);
  bool WriteNDEFMessage(android::Parcel& aParcel, const CommandOptions& aOptions);
  bool ReadTransceiveResponse(const android::Parcel& aParcel, EventOptions& aOptions);
private:
  nsTArray<nsString> mRequestIdQueue;
};

} // namespace mozilla

#endif // NfcMessageHandler_h
