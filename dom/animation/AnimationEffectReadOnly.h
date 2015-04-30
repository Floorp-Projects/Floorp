/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffect_h
#define mozilla_dom_AnimationEffect_h

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class AnimationEffectReadOnly
  : public nsISupports
  , public nsWrapperCache
{
protected:
  virtual ~AnimationEffectReadOnly() { }

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AnimationEffectReadOnly)

  explicit AnimationEffectReadOnly(nsISupports* aParent)
    : mParent(aParent)
  {
  }

  nsISupports* GetParentObject() const { return mParent; }

protected:
  nsCOMPtr<nsISupports> mParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffect_h
