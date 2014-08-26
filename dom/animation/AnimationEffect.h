/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffect_h
#define mozilla_dom_AnimationEffect_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Animation.h"

struct JSContext;

namespace mozilla {
namespace dom {

class AnimationEffect MOZ_FINAL : public nsWrapperCache
{
public:
  AnimationEffect(Animation* aAnimation)
    : mAnimation(aAnimation)
  {
    SetIsDOMBinding();
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationEffect)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationEffect)

  Animation* GetParentObject() const { return mAnimation; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // AnimationEffect interface
  void GetName(nsString& aRetVal) const {
    aRetVal = mAnimation->Name();
  }

private:
  ~AnimationEffect() { }

  nsRefPtr<Animation> mAnimation;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffect_h
