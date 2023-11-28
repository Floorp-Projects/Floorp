/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleWrap_h_
#define mozilla_a11y_AccessibleWrap_h_

#include "LocalAccessible.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/java/GeckoBundleWrappers.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace a11y {

class AccessibleWrap : public LocalAccessible {
 public:
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // TODO: Mark this as MOZ_CAN_RUN_SCRIPT
      virtual nsresult
      HandleAccEvent(AccEvent* aEvent) override;

  virtual void Shutdown() override;

  virtual bool DoAction(uint8_t aIndex) const override;

  static Accessible* ExploreByTouch(Accessible* aAccessible, float aX,
                                    float aY);

  static uint32_t GetFlags(role aRole, uint64_t aState, uint8_t aActionCount);

  static int32_t GetInputType(const nsString& aInputTypeAttr);

  static int32_t GetAndroidClass(role aRole);

  static void GetRoleDescription(role aRole, AccAttributes* aAttributes,
                                 nsAString& aGeckoRole,
                                 nsAString& aRoleDescription);

  static int32_t AndroidClass(Accessible* aAccessible);

  static int32_t GetVirtualViewID(Accessible* aAccessible);

  static void SetVirtualViewID(Accessible* aAccessible, int32_t aVirtualViewID);

  static Accessible* DoPivot(Accessible* aAccessible, int32_t aGranularity,
                             bool aForward, bool aInclusive);

  static Maybe<std::pair<int32_t, int32_t>> NavigateText(
      Accessible* aAccessible, int32_t aGranularity, int32_t aStartOffset,
      int32_t aEndOffset, bool aForward, bool aSelect);

 protected:
  int32_t mID;

 private:
  void GetTextEquiv(nsString& aText);

  bool HandleLiveRegionEvent(AccEvent* aEvent);
};

}  // namespace a11y
}  // namespace mozilla

#endif
