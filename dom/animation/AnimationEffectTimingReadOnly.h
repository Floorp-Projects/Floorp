/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffectTimingReadOnly_h
#define mozilla_dom_AnimationEffectTimingReadOnly_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TimingParams.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class AnimationEffectTimingReadOnly : public nsWrapperCache
{
public:
  AnimationEffectTimingReadOnly() = default;
  AnimationEffectTimingReadOnly(nsIDocument* aDocument,
                                const TimingParams& aTiming)
    : mDocument(aDocument)
    , mTiming(aTiming) { }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationEffectTimingReadOnly)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationEffectTimingReadOnly)

protected:
  virtual ~AnimationEffectTimingReadOnly() = default;

public:
  nsISupports* GetParentObject() const { return mDocument; }
  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  double Delay() const { return mTiming.mDelay.ToMilliseconds(); }
  double EndDelay() const { return mTiming.mEndDelay.ToMilliseconds(); }
  FillMode Fill() const { return mTiming.mFill; }
  double IterationStart() const { return mTiming.mIterationStart; }
  double Iterations() const { return mTiming.mIterations; }
  void GetDuration(OwningUnrestrictedDoubleOrString& aRetVal) const;
  PlaybackDirection Direction() const { return mTiming.mDirection; }
  void GetEasing(nsString& aRetVal) const;

  const TimingParams& AsTimingParams() const { return mTiming; }
  void SetTimingParams(const TimingParams& aTiming) { mTiming = aTiming; }

  virtual void Unlink() { }

protected:
  RefPtr<nsIDocument> mDocument;
  TimingParams mTiming;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffectTimingReadOnly_h
