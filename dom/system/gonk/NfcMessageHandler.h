/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcMessageHandler_h
#define NfcMessageHandler_h

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
};

} // namespace mozilla

#endif // NfcMessageHandler_h
