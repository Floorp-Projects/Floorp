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
#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {

class AccessibleWrap;
class AccAttributes;
class Accessible;
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
  static RefPtr<SessionAccessibility> GetInstanceFor(Accessible* aAccessible);

  // Native implementations
  using Base::AttachNative;
  using Base::DisposeNative;
  bool IsCacheEnabled();
  void GetNodeInfo(int32_t aID, mozilla::jni::Object::Param aNodeInfo);
  int GetNodeClassName(int32_t aID);
  void SetText(int32_t aID, jni::String::Param aText);
  void Click(int32_t aID);
  void Pivot(int32_t aID, int32_t aGranularity, bool aForward, bool aInclusive);
  bool CachedPivot(int32_t aID, int32_t aGranularity, bool aForward,
                   bool aInclusive);
  void ExploreByTouch(int32_t aID, float aX, float aY);
  void NavigateText(int32_t aID, int32_t aGranularity, int32_t aStartOffset,
                    int32_t aEndOffset, bool aForward, bool aSelect);
  void SetSelection(int32_t aID, int32_t aStart, int32_t aEnd);
  void Cut(int32_t aID);
  void Copy(int32_t aID);
  void Paste(int32_t aID);
  void StartNativeAccessibility();

  // Event methods
  void SendFocusEvent(Accessible* aAccessible);
  void SendScrollingEvent(Accessible* aAccessible, int32_t aScrollX,
                          int32_t aScrollY, int32_t aMaxScrollX,
                          int32_t aMaxScrollY);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void SendAccessibilityFocusedEvent(Accessible* aAccessible);
  void SendHoverEnterEvent(Accessible* aAccessible);
  void SendTextSelectionChangedEvent(Accessible* aAccessible,
                                     int32_t aCaretOffset);
  void SendTextTraversedEvent(Accessible* aAccessible, int32_t aStartOffset,
                              int32_t aEndOffset);
  void SendTextChangedEvent(Accessible* aAccessible, const nsString& aStr,
                            int32_t aStart, uint32_t aLen, bool aIsInsert,
                            bool aFromUser);
  void SendSelectedEvent(Accessible* aAccessible, bool aSelected);
  void SendClickedEvent(Accessible* aAccessible, uint32_t aFlags);
  void SendWindowContentChangedEvent();
  void SendWindowStateChangedEvent(Accessible* aAccessible);
  void SendAnnouncementEvent(Accessible* aAccessible,
                             const nsString& aAnnouncement, uint16_t aPriority);

  // Cache methods
  void ReplaceViewportCache(
      const nsTArray<Accessible*>& aAccessibles,
      const nsTArray<BatchData>& aData = nsTArray<BatchData>());

  void ReplaceFocusPathCache(
      const nsTArray<Accessible*>& aAccessibles,
      const nsTArray<BatchData>& aData = nsTArray<BatchData>());

  void UpdateCachedBounds(
      const nsTArray<Accessible*>& aAccessibles,
      const nsTArray<BatchData>& aData = nsTArray<BatchData>());

  void UpdateAccessibleFocusBoundaries(Accessible* aFirst, Accessible* aLast);

  Accessible* GetAccessibleByID(int32_t aID) const;

  static const int32_t kNoID = -1;
  static const int32_t kUnsetID = 0;

  static void RegisterAccessible(Accessible* aAccessible);
  static void UnregisterAccessible(Accessible* aAccessible);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SessionAccessibility)

 private:
  ~SessionAccessibility() {}

  mozilla::java::GeckoBundle::LocalRef ToBundle(Accessible* aAccessible,
                                                bool aSmall = false);

  mozilla::java::GeckoBundle::LocalRef ToBundle(
      Accessible* aAccessible, const uint64_t aState,
      const LayoutDeviceIntRect& aBounds, const uint8_t aActionCount,
      const nsString& aName, const nsString& aTextValue,
      const nsString& aDOMNodeID, const nsString& aDescription,
      const double& aCurVal = UnspecifiedNaN<double>(),
      const double& aMinVal = UnspecifiedNaN<double>(),
      const double& aMaxVal = UnspecifiedNaN<double>(),
      const double& aStep = UnspecifiedNaN<double>(),
      AccAttributes* aAttributes = nullptr);

  void PopulateNodeInfo(Accessible* aAccessible,
                        mozilla::jni::Object::Param aNodeInfo);

  void SetAttached(bool aAttached, already_AddRefed<Runnable> aRunnable);

  jni::NativeWeakPtr<widget::GeckoViewSupport> mWindow;  // Parent only
  java::SessionAccessibility::NativeProvider::GlobalRef mSessionAccessibility;

  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsTHashMap<nsUint32HashKey, Accessible*> mIDToAccessibleMap;
};

}  // namespace a11y
}  // namespace mozilla

#endif
