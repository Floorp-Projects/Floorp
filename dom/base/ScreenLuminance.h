/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScreenLuminance_h
#define mozilla_dom_ScreenLuminance_h

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

class nsScreen;

namespace mozilla {
namespace dom {

class ScreenLuminance final : public nsWrapperCache
{
public:
  // Ref counting and cycle collection
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ScreenLuminance)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ScreenLuminance)

  // WebIDL methods
  double Min() const { return mMin; }
  double Max() const { return mMax; }
  double MaxAverage() const { return mMaxAverage; }
  // End WebIDL methods

  ScreenLuminance(nsScreen* aScreen,
                  double aMin,
                  double aMax,
                  double aMaxAverage)
    : mScreen(aScreen)
    , mMin(aMin)
    , mMax(aMax)
    , mMaxAverage(aMaxAverage)
  {
  }

  nsScreen* GetParentObject() const { return mScreen; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  virtual ~ScreenLuminance() {}

  RefPtr<nsScreen> mScreen;
  double mMin;
  double mMax;
  double mMaxAverage;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScreenLuminance_h
