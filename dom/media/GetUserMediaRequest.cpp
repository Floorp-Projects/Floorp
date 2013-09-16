/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "GetUserMediaRequest.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsCxPusher.h"

namespace mozilla {
namespace dom {

GetUserMediaRequest::GetUserMediaRequest(
    nsPIDOMWindow* aInnerWindow,
    const nsAString& aCallID,
    const MediaStreamConstraintsInternal& aConstraints)
  : mInnerWindow(aInnerWindow)
  , mWindowID(aInnerWindow->GetOuterWindow()->WindowID())
  , mCallID(aCallID)
  , mConstraints(aConstraints)
{
  SetIsDOMBinding();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(GetUserMediaRequest, mInnerWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GetUserMediaRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GetUserMediaRequest)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GetUserMediaRequest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
GetUserMediaRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return GetUserMediaRequestBinding::Wrap(aCx, aScope, this);
}

nsISupports* GetUserMediaRequest::GetParentObject()
{
  return mInnerWindow;
}

void GetUserMediaRequest::GetCallID(nsString& retval)
{
  retval = mCallID;
}

uint64_t GetUserMediaRequest::WindowID()
{
  return mWindowID;
}

void
GetUserMediaRequest::GetConstraints(MediaStreamConstraintsInternal &result)
{
  result = mConstraints;
}

} // namespace dom
} // namespace mozilla
