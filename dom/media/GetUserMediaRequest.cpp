/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetUserMediaRequest.h"

#include "base/basictypes.h"
#include "MediaManager.h"
#include "mozilla/dom/MediaDevicesBinding.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"
#include "nsIMediaDevice.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"

namespace mozilla::dom {

GetUserMediaRequest::GetUserMediaRequest(
    nsPIDOMWindowInner* aInnerWindow, const nsAString& aCallID,
    RefPtr<MediaDeviceSetRefCnt> aMediaDeviceSet,
    const MediaStreamConstraints& aConstraints, bool aIsSecure,
    bool aIsHandlingUserInput)
    : mInnerWindowID(aInnerWindow->WindowID()),
      mOuterWindowID(aInnerWindow->GetOuterWindow()->WindowID()),
      mCallID(aCallID),
      mMediaDeviceSet(std::move(aMediaDeviceSet)),
      mConstraints(new MediaStreamConstraints(aConstraints)),
      mType(GetUserMediaRequestType::Getusermedia),
      mIsSecure(aIsSecure),
      mIsHandlingUserInput(aIsHandlingUserInput) {}

GetUserMediaRequest::GetUserMediaRequest(
    nsPIDOMWindowInner* aInnerWindow, const nsAString& aCallID,
    RefPtr<MediaDeviceSetRefCnt> aMediaDeviceSet,
    const AudioOutputOptions& aAudioOutputOptions, bool aIsSecure,
    bool aIsHandlingUserInput)
    : mInnerWindowID(aInnerWindow->WindowID()),
      mOuterWindowID(aInnerWindow->GetOuterWindow()->WindowID()),
      mCallID(aCallID),
      mMediaDeviceSet(std::move(aMediaDeviceSet)),
      mAudioOutputOptions(new AudioOutputOptions(aAudioOutputOptions)),
      mType(GetUserMediaRequestType::Selectaudiooutput),
      mIsSecure(aIsSecure),
      mIsHandlingUserInput(aIsHandlingUserInput) {}

GetUserMediaRequest::GetUserMediaRequest(nsPIDOMWindowInner* aInnerWindow,
                                         const nsAString& aRawId,
                                         const nsAString& aMediaSource,
                                         bool aIsHandlingUserInput)
    : mInnerWindowID(0),
      mOuterWindowID(0),
      mRawID(aRawId),
      mMediaSource(aMediaSource),
      mType(GetUserMediaRequestType::Recording_device_stopped),
      mIsSecure(false),
      mIsHandlingUserInput(aIsHandlingUserInput) {
  if (aInnerWindow && aInnerWindow->GetOuterWindow()) {
    mOuterWindowID = aInnerWindow->GetOuterWindow()->WindowID();
  }
}

GetUserMediaRequest::~GetUserMediaRequest() = default;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(GetUserMediaRequest)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GetUserMediaRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GetUserMediaRequest)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GetUserMediaRequest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* GetUserMediaRequest::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return GetUserMediaRequest_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* GetUserMediaRequest::GetParentObject() { return nullptr; }

GetUserMediaRequestType GetUserMediaRequest::Type() { return mType; }

void GetUserMediaRequest::GetCallID(nsString& retval) { retval = mCallID; }

void GetUserMediaRequest::GetRawID(nsString& retval) { retval = mRawID; }

void GetUserMediaRequest::GetMediaSource(nsString& retval) {
  retval = mMediaSource;
}

uint64_t GetUserMediaRequest::WindowID() { return mOuterWindowID; }

uint64_t GetUserMediaRequest::InnerWindowID() { return mInnerWindowID; }

bool GetUserMediaRequest::IsSecure() { return mIsSecure; }

bool GetUserMediaRequest::IsHandlingUserInput() const {
  return mIsHandlingUserInput;
}

void GetUserMediaRequest::GetDevices(
    nsTArray<RefPtr<nsIMediaDevice>>& retval) const {
  MOZ_ASSERT(retval.Length() == 0);
  if (!mMediaDeviceSet) {
    return;
  }
  for (const auto& device : *mMediaDeviceSet) {
    retval.AppendElement(device);
  }
}

void GetUserMediaRequest::GetConstraints(MediaStreamConstraints& result) {
  MOZ_ASSERT(result.mAudio.IsBoolean() && !result.mAudio.GetAsBoolean() &&
                 result.mVideo.IsBoolean() && !result.mVideo.GetAsBoolean(),
             "result should be default initialized");
  if (mConstraints) {
    result = *mConstraints;
  }
}

void GetUserMediaRequest::GetAudioOutputOptions(AudioOutputOptions& result) {
  MOZ_ASSERT(result.mDeviceId.IsEmpty(),
             "result should be default initialized");
  if (mAudioOutputOptions) {
    result = *mAudioOutputOptions;
  }
}

}  // namespace mozilla::dom
