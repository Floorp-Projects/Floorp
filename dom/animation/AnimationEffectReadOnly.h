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

struct ElementPropertyTransition;

namespace dom {

class Animation;
class AnimationEffectTimingReadOnly;
class KeyframeEffectReadOnly;
struct ComputedTimingProperties;

class AnimationEffectReadOnly : public nsISupports,
                                public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AnimationEffectReadOnly)

  explicit AnimationEffectReadOnly(nsIDocument* aDocument)
    : mDocument(aDocument)
  {
  }

  virtual KeyframeEffectReadOnly* AsKeyframeEffect() { return nullptr; }

  virtual ElementPropertyTransition* AsTransition() { return nullptr; }
  virtual const ElementPropertyTransition* AsTransition() const
  {
    return nullptr;
  }

  nsISupports* GetParentObject() const { return mDocument; }

  virtual bool IsInPlay() const = 0;
  virtual bool IsCurrent() const = 0;
  virtual bool IsInEffect() const = 0;

  virtual already_AddRefed<AnimationEffectTimingReadOnly> Timing() const = 0;
  virtual const TimingParams& SpecifiedTiming() const = 0;
  virtual void SetSpecifiedTiming(const TimingParams& aTiming) = 0;
  virtual void NotifyAnimationTimingUpdated() = 0;

  // Shortcut that gets the computed timing using the current local time as
  // calculated from the timeline time.
  virtual ComputedTiming GetComputedTiming(
    const TimingParams* aTiming = nullptr) const = 0;
  virtual void GetComputedTimingAsDict(
    ComputedTimingProperties& aRetVal) const = 0;

  virtual void SetAnimation(Animation* aAnimation) = 0;

protected:
  virtual ~AnimationEffectReadOnly() = default;

protected:
  RefPtr<nsIDocument> mDocument;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffectReadOnly_h
