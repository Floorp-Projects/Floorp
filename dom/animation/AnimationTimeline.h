/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationTimeline_h
#define mozilla_dom_AnimationTimeline_h

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "js/TypeDecls.h"
#include "nsIDocument.h"

struct JSContext;

namespace mozilla {
namespace dom {

class AnimationTimeline MOZ_FINAL : public nsWrapperCache
{
public:
  AnimationTimeline(nsIDocument* aDocument)
    : mDocument(aDocument)
  {
    SetIsDOMBinding();
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationTimeline)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationTimeline)

  nsISupports* GetParentObject() const { return mDocument; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  Nullable<double> GetCurrentTime() const;
  mozilla::TimeStamp GetCurrentTimeStamp() const;

  Nullable<double> ToTimelineTime(const mozilla::TimeStamp& aTimeStamp) const;

protected:
  virtual ~AnimationTimeline() { }

  nsCOMPtr<nsIDocument> mDocument;

  // Store the most recently returned value of current time. This is used
  // in cases where we don't have a refresh driver (e.g. because we are in
  // a display:none iframe).
  mutable mozilla::TimeStamp mLastCurrentTime;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationTimeline_h
