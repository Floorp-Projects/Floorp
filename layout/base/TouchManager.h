/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Description of TouchManager class.
 * Incapsulate code related with work of touch events.
 */

#ifndef TouchManager_h_
#define TouchManager_h_

#include "mozilla/BasicEvents.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/TouchEvents.h"
#include "nsRefPtrHashtable.h"

class nsIDocument;

namespace mozilla {
class PresShell;

class TouchManager {
public:
  // Initialize and release static variables
  static void InitializeStatics();
  static void ReleaseStatics();

  void Init(PresShell* aPresShell, nsIDocument* aDocument);
  void Destroy();

  bool PreHandleEvent(mozilla::WidgetEvent* aEvent,
                      nsEventStatus* aStatus,
                      bool& aTouchIsNew,
                      bool& aIsHandlingUserInput,
                      nsCOMPtr<nsIContent>& aCurrentEventContent);

  static already_AddRefed<nsIContent> GetAnyCapturedTouchTarget();
  static bool HasCapturedTouch(int32_t aId);
  static already_AddRefed<dom::Touch> GetCapturedTouch(int32_t aId);

private:
  void EvictTouches();
  static void EvictTouchPoint(RefPtr<dom::Touch>& aTouch,
                              nsIDocument* aLimitToDocument = nullptr);
  static void AppendToTouchList(WidgetTouchEvent::TouchArray* aTouchList);

  RefPtr<PresShell>   mPresShell;
  nsCOMPtr<nsIDocument> mDocument;

  struct TouchInfo
  {
    RefPtr<mozilla::dom::Touch> mTouch;
    nsCOMPtr<nsIContent> mNonAnonymousTarget;
  };
  static nsDataHashtable<nsUint32HashKey, TouchInfo>* sCaptureTouchList;
};

} // namespace mozilla

#endif /* !defined(TouchManager_h_) */
