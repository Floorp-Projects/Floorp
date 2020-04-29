/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PopupBlocker_h
#define mozilla_dom_PopupBlocker_h

#include "Element.h"
#include "mozilla/BasicEvents.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class PopupBlocker final {
 public:
  // Popup control state enum. The values in this enum must go from most
  // permissive to least permissive so that it's safe to push state in
  // all situations. Pushing popup state onto the stack never makes the
  // current popup state less permissive.
  // Keep this in sync with PopupBlockerState webidl dictionary!
  enum PopupControlState {
    openAllowed = 0,  // open that window without worries
    openControlled,   // it's a popup, but allow it
    openBlocked,      // it's a popup, but not from an allowed event
    openAbused,       // it's a popup. disallow it, but allow domain override.
    openOverridden    // disallow window open
  };

  static PopupControlState PushPopupControlState(PopupControlState aState,
                                                 bool aForce);

  static void PopPopupControlState(PopupControlState aState);

  static PopupControlState GetPopupControlState();

  static void PopupStatePusherCreated();
  static void PopupStatePusherDestroyed();

  // This method checks if the principal is allowed by open popups by user
  // permissions. In this case, the caller should not block popups.
  static bool CanShowPopupByPermission(nsIPrincipal* aPrincipal);

  // This method returns true if the caller is allowed to show a popup, and it
  // consumes the popup token for the current event. There is just 1 popup
  // allowed per event.
  // This method returns true if the token has been already consumed but
  // aPrincipal is the system principal.
  static bool TryUsePopupOpeningToken(nsIPrincipal* aPrincipal);

  static bool IsPopupOpeningTokenUnused();

  static PopupBlocker::PopupControlState GetEventPopupControlState(
      WidgetEvent* aEvent, Event* aDOMEvent = nullptr);

  // Returns if a external protocol iframe is allowed.
  static bool ConsumeTimerTokenForExternalProtocolIframe();

  // Returns when the last external protocol iframe has been allowed.
  static TimeStamp WhenLastExternalProtocolIframeAllowed();

  // Reset the last external protocol iframe timestamp.
  static void ResetLastExternalProtocolIframeAllowed();

  // These method track the number of popup which is considered as a spam popup.
  static void RegisterOpenPopupSpam();
  static void UnregisterOpenPopupSpam();
  static uint32_t GetOpenPopupSpamCount();

  static void Initialize();
  static void Shutdown();
};

}  // namespace dom
}  // namespace mozilla

#ifdef MOZILLA_INTERNAL_API
#  define AUTO_POPUP_STATE_PUSHER AutoPopupStatePusherInternal
#else
#  define AUTO_POPUP_STATE_PUSHER AutoPopupStatePusherExternal
#endif

// Helper class that helps with pushing and popping popup control
// state. Note that this class looks different from within code that's
// part of the layout library than it does in code outside the layout
// library.  We give the two object layouts different names so the symbols
// don't conflict, but code should always use the name
// |AutoPopupStatePusher|.
class MOZ_RAII AUTO_POPUP_STATE_PUSHER final {
 public:
#ifdef MOZILLA_INTERNAL_API
  explicit AUTO_POPUP_STATE_PUSHER(
      mozilla::dom::PopupBlocker::PopupControlState aState,
      bool aForce = false);
  ~AUTO_POPUP_STATE_PUSHER();
#else
  AUTO_POPUP_STATE_PUSHER(nsPIDOMWindowOuter* aWindow,
                          mozilla::dom::PopupBlocker::PopupControlState aState)
      : mWindow(aWindow), mOldState(openAbused) {
    if (aWindow) {
      mOldState = PopupBlocker::PushPopupControlState(aState, false);
    }
  }

  ~AUTO_POPUP_STATE_PUSHER() {
    if (mWindow) {
      PopupBlocker::PopPopupControlState(mOldState);
    }
  }
#endif

 protected:
#ifndef MOZILLA_INTERNAL_API
  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
#endif
  mozilla::dom::PopupBlocker::PopupControlState mOldState;
};

#define AutoPopupStatePusher AUTO_POPUP_STATE_PUSHER

#endif  // mozilla_PopupBlocker_h
