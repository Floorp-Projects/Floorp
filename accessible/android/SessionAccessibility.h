/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_SessionAccessibility_h_
#define mozilla_a11y_SessionAccessibility_h_

#include "mozilla/java/SessionAccessibilityNatives.h"
#include "mozilla/widget/GeckoViewSupport.h"
#include "nsAppShell.h"
#include "nsThreadUtils.h"
#include "nsWindow.h"

namespace mozilla {
namespace a11y {

class AccessibleWrap;
class RemoteAccessible;
class RootAccessibleWrap;
class BatchData;

class SessionAccessibility final
    : public java::SessionAccessibility::NativeProvider::Natives<
          SessionAccessibility> {
 public:
  typedef java::SessionAccessibility::NativeProvider::Natives<
      SessionAccessibility>
      Base;

  SessionAccessibility(
      jni::NativeWeakPtr<widget::GeckoViewSupport> aWindow,
      java::SessionAccessibility::NativeProvider::Param aSessionAccessibility);

  void OnWeakNonIntrusiveDetach(already_AddRefed<Runnable> aDisposer) {
    SetAttached(false, std::move(aDisposer));
  }

  const java::SessionAccessibility::NativeProvider::Ref&
  GetJavaAccessibility() {
    return mSessionAccessibility;
  }

  static void Init();
  static RefPtr<SessionAccessibility> GetInstanceFor(
      RemoteAccessible* aAccessible);
  static RefPtr<SessionAccessibility> GetInstanceFor(
      LocalAccessible* aAccessible);

  // Native implementations
  using Base::AttachNative;
  using Base::DisposeNative;
  jni::Object::LocalRef GetNodeInfo(int32_t aID);
  void SetText(int32_t aID, jni::String::Param aText);
  void Click(int32_t aID);
  void Pivot(int32_t aID, int32_t aGranularity, bool aForward, bool aInclusive);
  void ExploreByTouch(int32_t aID, float aX, float aY);
  void NavigateText(int32_t aID, int32_t aGranularity, int32_t aStartOffset,
                    int32_t aEndOffset, bool aForward, bool aSelect);
  void SetSelection(int32_t aID, int32_t aStart, int32_t aEnd);
  void Cut(int32_t aID);
  void Copy(int32_t aID);
  void Paste(int32_t aID);
  void StartNativeAccessibility();

  // Event methods
  void SendFocusEvent(AccessibleWrap* aAccessible);
  void SendScrollingEvent(AccessibleWrap* aAccessible, int32_t aScrollX,
                          int32_t aScrollY, int32_t aMaxScrollX,
                          int32_t aMaxScrollY);
  MOZ_CAN_RUN_SCRIPT
  void SendAccessibilityFocusedEvent(AccessibleWrap* aAccessible);
  void SendHoverEnterEvent(AccessibleWrap* aAccessible);
  void SendTextSelectionChangedEvent(AccessibleWrap* aAccessible,
                                     int32_t aCaretOffset);
  void SendTextTraversedEvent(AccessibleWrap* aAccessible, int32_t aStartOffset,
                              int32_t aEndOffset);
  void SendTextChangedEvent(AccessibleWrap* aAccessible, const nsString& aStr,
                            int32_t aStart, uint32_t aLen, bool aIsInsert,
                            bool aFromUser);
  void SendSelectedEvent(AccessibleWrap* aAccessible, bool aSelected);
  void SendClickedEvent(AccessibleWrap* aAccessible, uint32_t aFlags);
  void SendWindowContentChangedEvent();
  void SendWindowStateChangedEvent(AccessibleWrap* aAccessible);
  void SendAnnouncementEvent(AccessibleWrap* aAccessible,
                             const nsString& aAnnouncement, uint16_t aPriority);

  // Cache methods
  void ReplaceViewportCache(
      const nsTArray<AccessibleWrap*>& aAccessibles,
      const nsTArray<BatchData>& aData = nsTArray<BatchData>());

  void ReplaceFocusPathCache(
      const nsTArray<AccessibleWrap*>& aAccessibles,
      const nsTArray<BatchData>& aData = nsTArray<BatchData>());

  void UpdateCachedBounds(
      const nsTArray<AccessibleWrap*>& aAccessibles,
      const nsTArray<BatchData>& aData = nsTArray<BatchData>());

  void UpdateAccessibleFocusBoundaries(AccessibleWrap* aFirst,
                                       AccessibleWrap* aLast);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SessionAccessibility)

 private:
  ~SessionAccessibility() {}

  void SetAttached(bool aAttached, already_AddRefed<Runnable> aRunnable);
  RootAccessibleWrap* GetRoot();

  jni::NativeWeakPtr<widget::GeckoViewSupport> mWindow;  // Parent only
  java::SessionAccessibility::NativeProvider::GlobalRef mSessionAccessibility;
};

}  // namespace a11y
}  // namespace mozilla

#endif
