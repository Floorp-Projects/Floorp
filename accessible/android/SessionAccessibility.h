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

class Accessible;

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
  static RefPtr<SessionAccessibility> GetInstanceFor(PresShell* aPresShell);

  // Native implementations
  using Base::AttachNative;
  using Base::DisposeNative;
  void GetNodeInfo(int32_t aID, mozilla::jni::Object::Param aNodeInfo);
  int GetNodeClassName(int32_t aID);
  void SetText(int32_t aID, jni::String::Param aText);
  void Click(int32_t aID);
  bool Pivot(int32_t aID, int32_t aGranularity, bool aForward, bool aInclusive);
  void ExploreByTouch(int32_t aID, float aX, float aY);
  bool NavigateText(int32_t aID, int32_t aGranularity, int32_t aStartOffset,
                    int32_t aEndOffset, bool aForward, bool aSelect);
  void SetSelection(int32_t aID, int32_t aStart, int32_t aEnd);
  void Cut(int32_t aID);
  void Copy(int32_t aID);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Paste(int32_t aID);
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
  void SendTextChangedEvent(Accessible* aAccessible, const nsAString& aStr,
                            int32_t aStart, uint32_t aLen, bool aIsInsert,
                            bool aFromUser);
  void SendSelectedEvent(Accessible* aAccessible, bool aSelected);
  void SendClickedEvent(Accessible* aAccessible, uint32_t aFlags);
  void SendWindowContentChangedEvent();
  void SendWindowStateChangedEvent(Accessible* aAccessible);
  void SendAnnouncementEvent(Accessible* aAccessible,
                             const nsAString& aAnnouncement,
                             uint16_t aPriority);

  Accessible* GetAccessibleByID(int32_t aID) const;

  static const int32_t kNoID = -1;
  static const int32_t kUnsetID = 0;

  static void RegisterAccessible(Accessible* aAccessible);
  static void UnregisterAccessible(Accessible* aAccessible);
  static void UnregisterAll(PresShell* aPresShell);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SessionAccessibility)

 private:
  ~SessionAccessibility() {}

  void PopulateNodeInfo(Accessible* aAccessible,
                        mozilla::jni::Object::Param aNodeInfo);

  void SetAttached(bool aAttached, already_AddRefed<Runnable> aRunnable);

  bool DoNavigateText(Accessible* aAccessible, int32_t aGranularity,
                      int32_t aStartOffset, int32_t aEndOffset, bool aForward,
                      bool aSelect);

  jni::NativeWeakPtr<widget::GeckoViewSupport> mWindow;  // Parent only
  java::SessionAccessibility::NativeProvider::GlobalRef mSessionAccessibility;

  class IDMappingEntry {
   public:
    explicit IDMappingEntry(Accessible* aAccessible);

    IDMappingEntry& operator=(Accessible* aAccessible);

    operator Accessible*() const;

   private:
    // A strong reference to a DocAccessible or DocAccessibleParent. They don't
    // share any useful base class except nsISupports, so we use that.
    // When we retrieve the document from this reference we cast it to
    // LocalAccessible in the DocAccessible case because DocAccessible has
    // multiple inheritance paths for nsISupports.
    RefPtr<nsISupports> mDoc;
    // The ID of the accessible as used in the internal doc mapping.
    // We rely on this ID being pointer derived and therefore divisible by two
    // so we can use the first bit to mark if it is remote or not.
    uint64_t mInternalID;

    static const uintptr_t IS_REMOTE = 0x1;
  };

  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsBaseHashtable<nsUint32HashKey, IDMappingEntry, Accessible*>
      mIDToAccessibleMap;
};

}  // namespace a11y
}  // namespace mozilla

#endif
