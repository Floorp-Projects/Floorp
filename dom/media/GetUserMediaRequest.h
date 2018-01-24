/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GetUserMediaRequest_h__
#define GetUserMediaRequest_h__

#include "mozilla/ErrorResult.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

struct MediaStreamConstraints;

class GetUserMediaRequest : public nsISupports, public nsWrapperCache
{
public:
  GetUserMediaRequest(nsPIDOMWindowInner* aInnerWindow,
                      const nsAString& aCallID,
                      const MediaStreamConstraints& aConstraints,
                      bool aIsSecure,
                      bool aIsHandlingUserInput);
  GetUserMediaRequest(nsPIDOMWindowInner* aInnerWindow,
                      const nsAString& aRawId,
                      const nsAString& aMediaSource);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GetUserMediaRequest)

  JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject();

  uint64_t WindowID();
  uint64_t InnerWindowID();
  bool IsSecure();
  bool IsHandlingUserInput() const;
  void GetCallID(nsString& retval);
  void GetRawID(nsString& retval);
  void GetMediaSource(nsString& retval);
  void GetConstraints(MediaStreamConstraints &result);

private:
  virtual ~GetUserMediaRequest() {}

  uint64_t mInnerWindowID, mOuterWindowID;
  const nsString mCallID;
  const nsString mRawID;
  const nsString mMediaSource;
  nsAutoPtr<MediaStreamConstraints> mConstraints;
  bool mIsSecure;
  bool mIsHandlingUserInput;
};

} // namespace dom
} // namespace mozilla

#endif // GetUserMediaRequest_h__
