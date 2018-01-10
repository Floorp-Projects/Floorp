/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "GetUserMediaRequest.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

GetUserMediaRequest::GetUserMediaRequest(
    nsPIDOMWindowInner* aInnerWindow,
    const nsAString& aCallID,
    const MediaStreamConstraints& aConstraints,
    bool aIsSecure,
    bool aIsHandlingUserInput)
  : mInnerWindowID(aInnerWindow->WindowID())
  , mOuterWindowID(aInnerWindow->GetOuterWindow()->WindowID())
  , mCallID(aCallID)
  , mConstraints(new MediaStreamConstraints(aConstraints))
  , mIsSecure(aIsSecure)
  , mIsHandlingUserInput(aIsHandlingUserInput)
{
}

GetUserMediaRequest::GetUserMediaRequest(
    nsPIDOMWindowInner* aInnerWindow,
    const nsAString& aRawId,
    const nsAString& aMediaSource)
  : mRawID(aRawId)
  , mMediaSource(aMediaSource)
{
  if (aInnerWindow && aInnerWindow->GetOuterWindow()) {
    mOuterWindowID = aInnerWindow->GetOuterWindow()->WindowID();
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(GetUserMediaRequest)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GetUserMediaRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GetUserMediaRequest)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GetUserMediaRequest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
GetUserMediaRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GetUserMediaRequestBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports* GetUserMediaRequest::GetParentObject()
{
  return nullptr;
}

void GetUserMediaRequest::GetCallID(nsString& retval)
{
  retval = mCallID;
}

void GetUserMediaRequest::GetRawID(nsString& retval)
{
  retval = mRawID;
}

void GetUserMediaRequest::GetMediaSource(nsString& retval)
{
  retval = mMediaSource;
}

uint64_t GetUserMediaRequest::WindowID()
{
  return mOuterWindowID;
}

uint64_t GetUserMediaRequest::InnerWindowID()
{
  return mInnerWindowID;
}

bool GetUserMediaRequest::IsSecure()
{
  return mIsSecure;
}

bool GetUserMediaRequest::IsHandlingUserInput() const
{
  return mIsHandlingUserInput;
}

void
GetUserMediaRequest::GetConstraints(MediaStreamConstraints &result)
{
  result = *mConstraints;
}

} // namespace dom
} // namespace mozilla
