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
class MediaStreamConstraintsInternal;

class GetUserMediaRequest : public nsISupports, public nsWrapperCache
{
public:
  GetUserMediaRequest(nsPIDOMWindow* aInnerWindow,
                      const nsAString& aCallID,
                      const MediaStreamConstraintsInternal& aConstraints);
  virtual ~GetUserMediaRequest() {};

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GetUserMediaRequest)

  virtual JSObject* WrapObject(JSContext* cx)
    MOZ_OVERRIDE;
  nsISupports* GetParentObject();

  uint64_t WindowID();
  uint64_t InnerWindowID();
  void GetCallID(nsString& retval);
  void GetConstraints(MediaStreamConstraintsInternal &result);

private:
  uint64_t mInnerWindowID, mOuterWindowID;
  const nsString mCallID;
  nsAutoPtr<MediaStreamConstraintsInternal> mConstraints;
};

} // namespace dom
} // namespace mozilla

#endif // GetUserMediaRequest_h__
