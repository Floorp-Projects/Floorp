/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffectReadOnly_h
#define mozilla_dom_AnimationEffectReadOnly_h

#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class AnimationEffectTimingReadOnly;
struct ComputedTimingProperties;

class AnimationEffectReadOnly : public nsISupports,
                                public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AnimationEffectReadOnly)

  explicit AnimationEffectReadOnly(nsISupports* aParent)
    : mParent(aParent)
  {
  }

  nsISupports* GetParentObject() const { return mParent; }

  virtual already_AddRefed<AnimationEffectTimingReadOnly> Timing() const = 0;

  virtual void GetComputedTimingAsDict(ComputedTimingProperties& aRetVal) const = 0;

protected:
  virtual ~AnimationEffectReadOnly() = default;

protected:
  nsCOMPtr<nsISupports> mParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffectReadOnly_h
