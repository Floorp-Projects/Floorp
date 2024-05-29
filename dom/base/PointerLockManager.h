/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PointerLockManager_h
#define mozilla_PointerLockManager_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsIWeakReferenceUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
enum class StyleCursorKind : uint8_t;

namespace dom {
class BrowsingContext;
class BrowserParent;
enum class CallerType : uint32_t;
class Document;
class Element;
}  // namespace dom

class PointerLockManager final {
 public:
  static void RequestLock(dom::Element* aElement, dom::CallerType aCallerType);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static void Unlock(dom::Document* aDoc = nullptr);

  static bool IsLocked() { return sIsLocked; }

  static already_AddRefed<dom::Element> GetLockedElement();

  static already_AddRefed<dom::Document> GetLockedDocument();

  static dom::BrowserParent* GetLockedRemoteTarget();

  /**
   * Returns true if aContext and the current pointer locked document
   * have common top BrowsingContext.
   * Note that this method returns true only if caller is in the same process
   * as pointer locked document.
   */
  static bool IsInLockContext(mozilla::dom::BrowsingContext* aContext);

  // Set/release pointer lock remote target. Should only be called in parent
  // process.
  static void SetLockedRemoteTarget(dom::BrowserParent* aBrowserParent,
                                    nsACString& aError);
  static void ReleaseLockedRemoteTarget(dom::BrowserParent* aBrowserParent);

 private:
  class PointerLockRequest final : public Runnable {
   public:
    PointerLockRequest(dom::Element* aElement, bool aUserInputOrChromeCaller);
    MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() final;

   private:
    nsWeakPtr mElement;
    nsWeakPtr mDocument;
    bool mUserInputOrChromeCaller;
  };

  static void ChangePointerLockedElement(dom::Element* aElement,
                                         dom::Document* aDocument,
                                         dom::Element* aPointerLockedElement);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static bool StartSetPointerLock(dom::Element* aElement,
                                  dom::Document* aDocument);

  MOZ_CAN_RUN_SCRIPT
  static bool SetPointerLock(dom::Element* aElement, dom::Document* aDocument,
                             StyleCursorKind);

  static bool sIsLocked;
};

}  // namespace mozilla

#endif  // mozilla_PointerLockManager_h
