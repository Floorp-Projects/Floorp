/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GetUserMediaRequest_h__
#define GetUserMediaRequest_h__

#include <cstdint>
#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"
#include "mozilla/UniquePtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsIMediaDevice;
class nsPIDOMWindowInner;

namespace mozilla {

class MediaDevice;

namespace media {
template <typename T>
class Refcountable;
}

namespace dom {

struct AudioOutputOptions;
struct MediaStreamConstraints;
enum class GetUserMediaRequestType : uint8_t;

class GetUserMediaRequest : public nsISupports, public nsWrapperCache {
 public:
  using MediaDeviceSetRefCnt =
      media::Refcountable<nsTArray<RefPtr<MediaDevice>>>;

  // For getUserMedia "getUserMedia:request"
  GetUserMediaRequest(nsPIDOMWindowInner* aInnerWindow,
                      const nsAString& aCallID,
                      RefPtr<MediaDeviceSetRefCnt> aMediaDeviceSet,
                      const MediaStreamConstraints& aConstraints,
                      bool aIsSecure, bool aIsHandlingUserInput);
  // For selectAudioOutput "getUserMedia:request"
  GetUserMediaRequest(nsPIDOMWindowInner* aInnerWindow,
                      const nsAString& aCallID,
                      RefPtr<MediaDeviceSetRefCnt> aMediaDeviceSet,
                      const AudioOutputOptions& aAudioOutputOptions,
                      bool aIsSecure, bool aIsHandlingUserInput);
  // For "recording-device-stopped"
  GetUserMediaRequest(nsPIDOMWindowInner* aInnerWindow, const nsAString& aRawId,
                      const nsAString& aMediaSource, bool aIsHandlingUserInput);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GetUserMediaRequest)

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject();

  GetUserMediaRequestType Type();
  uint64_t WindowID();
  uint64_t InnerWindowID();
  bool IsSecure();
  bool IsHandlingUserInput() const;
  void GetCallID(nsString& retval);
  void GetRawID(nsString& retval);
  void GetMediaSource(nsString& retval);
  void GetDevices(nsTArray<RefPtr<nsIMediaDevice>>& retval) const;
  void GetConstraints(MediaStreamConstraints& result);
  void GetAudioOutputOptions(AudioOutputOptions& result);

 private:
  virtual ~GetUserMediaRequest();

  uint64_t mInnerWindowID, mOuterWindowID;
  const nsString mCallID;
  const nsString mRawID;
  const nsString mMediaSource;
  const RefPtr<MediaDeviceSetRefCnt> mMediaDeviceSet;
  UniquePtr<MediaStreamConstraints> mConstraints;
  UniquePtr<AudioOutputOptions> mAudioOutputOptions;
  GetUserMediaRequestType mType;
  bool mIsSecure;
  bool mIsHandlingUserInput;
};

}  // namespace dom
}  // namespace mozilla

#endif  // GetUserMediaRequest_h__
