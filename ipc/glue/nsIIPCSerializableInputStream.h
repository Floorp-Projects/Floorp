/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_nsIIPCSerializableInputStream_h
#define mozilla_ipc_nsIIPCSerializableInputStream_h

#include "nsISupports.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace ipc {
class InputStreamParams;
}
}

#define NS_IIPCSERIALIZABLEINPUTSTREAM_IID \
  {0x1f56a3f8, 0xc413, 0x4274, {0x88, 0xe6, 0x68, 0x50, 0x9d, 0xf8, 0x85, 0x2d}}

class NS_NO_VTABLE nsIIPCSerializableInputStream : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

  virtual void
  Serialize(mozilla::ipc::InputStreamParams& aParams) = 0;

  virtual bool
  Deserialize(const mozilla::ipc::InputStreamParams& aParams) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIIPCSerializableInputStream,
                              NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

#define NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM \
  virtual void \
  Serialize(mozilla::ipc::InputStreamParams&) MOZ_OVERRIDE; \
  virtual bool \
  Deserialize(const mozilla::ipc::InputStreamParams&) MOZ_OVERRIDE;

#define NS_FORWARD_NSIIPCSERIALIZABLEINPUTSTREAM(_to) \
  virtual void \
  Serialize(mozilla::ipc::InputStreamParams& aParams) MOZ_OVERRIDE \
  { _to Serialize(aParams); } \
  virtual bool \
  Deserialize(const mozilla::ipc::InputStreamParams& aParams) MOZ_OVERRIDE \
  { return _to Deserialize(aParams); }

#define NS_FORWARD_SAFE_NSIIPCSERIALIZABLEINPUTSTREAM(_to) \
  virtual void \
  Serialize(mozilla::ipc::InputStreamParams& aParams) MOZ_OVERRIDE \
  { if (_to) { _to->Serialize(aParams); } } \
  virtual bool \
  Deserialize(const mozilla::ipc::InputStreamParams& aParams) MOZ_OVERRIDE \
  { if (_to) { return _to->Deserialize(aParams); } return false; }

#endif // mozilla_ipc_nsIIPCSerializableInputStream_h
