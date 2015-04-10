/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationTimeline_h
#define mozilla_dom_AnimationTimeline_h

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/Attributes.h"
#include "nsIGlobalObject.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class AnimationTimeline
  : public nsISupports
  , public nsWrapperCache
{
public:
  explicit AnimationTimeline(nsIGlobalObject* aWindow)
    : mWindow(aWindow)
  {
    MOZ_ASSERT(mWindow);
  }

protected:
  virtual ~AnimationTimeline() { }

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AnimationTimeline)

  nsIGlobalObject* GetParentObject() const { return mWindow; }

  // AnimationTimeline methods
  virtual Nullable<TimeDuration> GetCurrentTime() const = 0;

  // Wrapper functions for AnimationTimeline DOM methods when called from
  // script.
  Nullable<double> GetCurrentTimeAsDouble() const {
    return AnimationUtils::TimeDurationToDouble(GetCurrentTime());
  }

protected:
  nsCOMPtr<nsIGlobalObject> mWindow;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationTimeline_h
