/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaDeviceInfo.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/MediaManager.h"
#include "nsIScriptGlobalObject.h"

namespace mozilla {
namespace dom {

MediaDeviceInfo::MediaDeviceInfo(const nsAString& aDeviceId,
                                 MediaDeviceKind aKind,
                                 const nsAString& aLabel,
                                 const nsAString& aGroupId)
  : mKind(aKind)
  , mDeviceId(aDeviceId)
  , mLabel(aLabel)
  , mGroupId(aGroupId) {}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(MediaDeviceInfo)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaDeviceInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaDeviceInfo)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaDeviceInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
MediaDeviceInfo::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaDeviceInfoBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports* MediaDeviceInfo::GetParentObject()
{
  return nullptr;
}

void MediaDeviceInfo::GetDeviceId(nsString& retval)
{
  retval = mDeviceId;
}

MediaDeviceKind
MediaDeviceInfo::Kind()
{
  return mKind;
}

void MediaDeviceInfo::GetGroupId(nsString& retval)
{
  retval = mGroupId;
}

void MediaDeviceInfo::GetLabel(nsString& retval)
{
  retval = mLabel;
}

MediaDeviceKind Kind();
void GetLabel(nsString& retval);
void GetGroupId(nsString& retval);

} // namespace dom
} // namespace mozilla
