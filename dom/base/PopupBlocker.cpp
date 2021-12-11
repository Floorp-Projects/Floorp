/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Components.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TimeStamp.h"
#include "nsXULPopupManager.h"
#include "nsIPermissionManager.h"

namespace mozilla::dom {

namespace {

static char* sPopupAllowedEvents;

static PopupBlocker::PopupControlState sPopupControlState =
    PopupBlocker::openAbused;
static uint32_t sPopupStatePusherCount = 0;

static TimeStamp sLastAllowedExternalProtocolIFrameTimeStamp;

static uint32_t sOpenPopupSpamCount = 0;

void PopupAllowedEventsChanged() {
  if (sPopupAllowedEvents) {
    free(sPopupAllowedEvents);
  }

  nsAutoCString str;
  Preferences::GetCString("dom.popup_allowed_events", str);

  // We'll want to do this even if str is empty to avoid looking up
  // this pref all the time if it's not set.
  sPopupAllowedEvents = ToNewCString(str);
}

// return true if eventName is contained within events, delimited by
// spaces
bool PopupAllowedForEvent(const char* eventName) {
  if (!sPopupAllowedEvents) {
    PopupAllowedEventsChanged();

    if (!sPopupAllowedEvents) {
      return false;
    }
  }

  nsDependentCString events(sPopupAllowedEvents);

  nsCString::const_iterator start, end;
  nsCString::const_iterator startiter(events.BeginReading(start));
  events.EndReading(end);

  while (startiter != end) {
    nsCString::const_iterator enditer(end);

    if (!FindInReadable(nsDependentCString(eventName), startiter, enditer))
      return false;

    // the match is surrounded by spaces, or at a string boundary
    if ((startiter == start || *--startiter == ' ') &&
        (enditer == end || *enditer == ' ')) {
      return true;
    }

    // Move on and see if there are other matches. (The delimitation
    // requirement makes it pointless to begin the next search before
    // the end of the invalid match just found.)
    startiter = enditer;
  }

  return false;
}

// static
void OnPrefChange(const char* aPrefName, void*) {
  nsDependentCString prefName(aPrefName);
  if (prefName.EqualsLiteral("dom.popup_allowed_events")) {
    PopupAllowedEventsChanged();
  }
}

}  // namespace

/* static */
PopupBlocker::PopupControlState PopupBlocker::PushPopupControlState(
    PopupBlocker::PopupControlState aState, bool aForce) {
  MOZ_ASSERT(NS_IsMainThread());
  PopupBlocker::PopupControlState old = sPopupControlState;
  if (aState < old || aForce) {
    sPopupControlState = aState;
  }
  return old;
}

/* static */
void PopupBlocker::PopPopupControlState(
    PopupBlocker::PopupControlState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  sPopupControlState = aState;
}

/* static */ PopupBlocker::PopupControlState
PopupBlocker::GetPopupControlState() {
  return sPopupControlState;
}

/* static */
uint32_t PopupBlocker::GetPopupPermission(nsIPrincipal* aPrincipal) {
  uint32_t permit = nsIPermissionManager::UNKNOWN_ACTION;
  nsCOMPtr<nsIPermissionManager> permissionManager =
      components::PermissionManager::Service();

  if (permissionManager) {
    permissionManager->TestPermissionFromPrincipal(aPrincipal, "popup"_ns,
                                                   &permit);
  }

  return permit;
}

/* static */
void PopupBlocker::PopupStatePusherCreated() { ++sPopupStatePusherCount; }

/* static */
void PopupBlocker::PopupStatePusherDestroyed() {
  MOZ_ASSERT(sPopupStatePusherCount);
  --sPopupStatePusherCount;
}

// static
PopupBlocker::PopupControlState PopupBlocker::GetEventPopupControlState(
    WidgetEvent* aEvent, Event* aDOMEvent) {
  // generally if an event handler is running, new windows are disallowed.
  // check for exceptions:
  PopupBlocker::PopupControlState abuse = PopupBlocker::openBlocked;

  if (aDOMEvent && aDOMEvent->GetWantsPopupControlCheck()) {
    nsAutoString type;
    aDOMEvent->GetType(type);
    if (PopupAllowedForEvent(NS_ConvertUTF16toUTF8(type).get())) {
      return PopupBlocker::openAllowed;
    }
  }

  switch (aEvent->mClass) {
    case eBasicEventClass:
      // For these following events only allow popups if they're
      // triggered while handling user input. See
      // UserActivation::IsUserInteractionEvent() for details.
      if (UserActivation::IsHandlingUserInput()) {
        switch (aEvent->mMessage) {
          case eFormSelect:
            if (PopupAllowedForEvent("select")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          case eFormChange:
            if (PopupAllowedForEvent("change")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          default:
            break;
        }
      }
      break;
    case eEditorInputEventClass:
      // For this following event only allow popups if it's triggered
      // while handling user input. See
      // UserActivation::IsUserInteractionEvent() for details.
      if (UserActivation::IsHandlingUserInput()) {
        switch (aEvent->mMessage) {
          case eEditorInput:
            if (PopupAllowedForEvent("input")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          default:
            break;
        }
      }
      break;
    case eInputEventClass:
      // For this following event only allow popups if it's triggered
      // while handling user input. See
      // UserActivation::IsUserInteractionEvent() for details.
      if (UserActivation::IsHandlingUserInput()) {
        switch (aEvent->mMessage) {
          case eFormChange:
            if (PopupAllowedForEvent("change")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          case eXULCommand:
            abuse = PopupBlocker::openControlled;
            break;
          default:
            break;
        }
      }
      break;
    case eKeyboardEventClass:
      if (aEvent->IsTrusted()) {
        uint32_t key = aEvent->AsKeyboardEvent()->mKeyCode;
        switch (aEvent->mMessage) {
          case eKeyPress:
            // return key on focused button. see note at eMouseClick.
            if (key == NS_VK_RETURN) {
              abuse = PopupBlocker::openAllowed;
            } else if (PopupAllowedForEvent("keypress")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          case eKeyUp:
            // space key on focused button. see note at eMouseClick.
            if (key == NS_VK_SPACE) {
              abuse = PopupBlocker::openAllowed;
            } else if (PopupAllowedForEvent("keyup")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          case eKeyDown:
            if (PopupAllowedForEvent("keydown")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          default:
            break;
        }
      }
      break;
    case eTouchEventClass:
      if (aEvent->IsTrusted()) {
        switch (aEvent->mMessage) {
          case eTouchStart:
            if (PopupAllowedForEvent("touchstart")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          case eTouchEnd:
            if (PopupAllowedForEvent("touchend")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          default:
            break;
        }
      }
      break;
    case eMouseEventClass:
      if (aEvent->IsTrusted()) {
        // Let's ignore MouseButton::eSecondary because that is handled as
        // context menu.
        if (aEvent->AsMouseEvent()->mButton == MouseButton::ePrimary ||
            aEvent->AsMouseEvent()->mButton == MouseButton::eMiddle) {
          switch (aEvent->mMessage) {
            case eMouseUp:
              if (PopupAllowedForEvent("mouseup")) {
                abuse = PopupBlocker::openControlled;
              }
              break;
            case eMouseDown:
              if (PopupAllowedForEvent("mousedown")) {
                abuse = PopupBlocker::openControlled;
              }
              break;
            case eMouseClick:
              /* Click events get special treatment because of their
                 historical status as a more legitimate event handler. If
                 click popups are enabled in the prefs, clear the popup
                 status completely. */
              if (PopupAllowedForEvent("click")) {
                abuse = PopupBlocker::openAllowed;
              }
              break;
            case eMouseDoubleClick:
              if (PopupAllowedForEvent("dblclick")) {
                abuse = PopupBlocker::openControlled;
              }
              break;
            default:
              break;
          }
        } else if (aEvent->mMessage == eMouseAuxClick) {
          // Not eLeftButton
          // There's not a strong reason to ignore other events (eg eMouseUp)
          // for non-primary clicks as far as we know, so we could add them if
          // it becomes a compat issue
          if (PopupAllowedForEvent("auxclick")) {
            abuse = PopupBlocker::openControlled;
          }
        }

        switch (aEvent->mMessage) {
          case eContextMenu:
            if (PopupAllowedForEvent("contextmenu")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          default:
            break;
        }
      }
      break;
    case ePointerEventClass:
      if (aEvent->IsTrusted() &&
          (aEvent->AsPointerEvent()->mButton == MouseButton::ePrimary ||
           aEvent->AsPointerEvent()->mButton == MouseButton::eMiddle)) {
        switch (aEvent->mMessage) {
          case ePointerUp:
            if (PopupAllowedForEvent("pointerup")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          case ePointerDown:
            if (PopupAllowedForEvent("pointerdown")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          default:
            break;
        }
      }
      break;
    case eFormEventClass:
      // For these following events only allow popups if they're
      // triggered while handling user input. See
      // UserActivation::IsUserInteractionEvent() for details.
      if (UserActivation::IsHandlingUserInput()) {
        switch (aEvent->mMessage) {
          case eFormSubmit:
            if (PopupAllowedForEvent("submit")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          case eFormReset:
            if (PopupAllowedForEvent("reset")) {
              abuse = PopupBlocker::openControlled;
            }
            break;
          default:
            break;
        }
      }
      break;
    default:
      break;
  }

  return abuse;
}

/* static */
void PopupBlocker::Initialize() {
  DebugOnly<nsresult> rv =
      Preferences::RegisterCallback(OnPrefChange, "dom.popup_allowed_events");
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to observe \"dom.popup_allowed_events\"");
}

/* static */
void PopupBlocker::Shutdown() {
  MOZ_ASSERT(sOpenPopupSpamCount == 0);

  if (sPopupAllowedEvents) {
    free(sPopupAllowedEvents);
  }

  Preferences::UnregisterCallback(OnPrefChange, "dom.popup_allowed_events");
}

/* static */
bool PopupBlocker::ConsumeTimerTokenForExternalProtocolIframe() {
  if (!StaticPrefs::dom_delay_block_external_protocol_in_iframes_enabled()) {
    return false;
  }

  TimeStamp now = TimeStamp::Now();

  if (sLastAllowedExternalProtocolIFrameTimeStamp.IsNull()) {
    sLastAllowedExternalProtocolIFrameTimeStamp = now;
    return true;
  }

  if ((now - sLastAllowedExternalProtocolIFrameTimeStamp).ToSeconds() <
      StaticPrefs::dom_delay_block_external_protocol_in_iframes()) {
    return false;
  }

  sLastAllowedExternalProtocolIFrameTimeStamp = now;
  return true;
}

/* static */
TimeStamp PopupBlocker::WhenLastExternalProtocolIframeAllowed() {
  return sLastAllowedExternalProtocolIFrameTimeStamp;
}

/* static */
void PopupBlocker::ResetLastExternalProtocolIframeAllowed() {
  sLastAllowedExternalProtocolIFrameTimeStamp = TimeStamp();
}

/* static */
void PopupBlocker::RegisterOpenPopupSpam() { sOpenPopupSpamCount++; }

/* static */
void PopupBlocker::UnregisterOpenPopupSpam() {
  MOZ_ASSERT(sOpenPopupSpamCount);
  sOpenPopupSpamCount--;
}

/* static */
uint32_t PopupBlocker::GetOpenPopupSpamCount() { return sOpenPopupSpamCount; }

}  // namespace mozilla::dom

AutoPopupStatePusherInternal::AutoPopupStatePusherInternal(
    mozilla::dom::PopupBlocker::PopupControlState aState, bool aForce)
    : mOldState(
          mozilla::dom::PopupBlocker::PushPopupControlState(aState, aForce)) {
  mozilla::dom::PopupBlocker::PopupStatePusherCreated();
}

AutoPopupStatePusherInternal::~AutoPopupStatePusherInternal() {
  mozilla::dom::PopupBlocker::PopPopupControlState(mOldState);
  mozilla::dom::PopupBlocker::PopupStatePusherDestroyed();
}
