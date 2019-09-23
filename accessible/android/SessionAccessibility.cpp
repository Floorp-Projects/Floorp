/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionAccessibility.h"
#include "AndroidUiThread.h"
#include "DocAccessibleParent.h"
#include "nsThreadUtils.h"
#include "AccessibilityEvent.h"
#include "HyperTextAccessible.h"
#include "JavaBuiltins.h"
#include "RootAccessibleWrap.h"
#include "nsAccessibilityService.h"
#include "nsViewManager.h"
#include "nsIPersistentProperties2.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/jni/GeckoBundleUtils.h"

#ifdef DEBUG
#  include <android/log.h>
#  define AALOG(args...) \
    __android_log_print(ANDROID_LOG_INFO, "GeckoAccessibilityNative", ##args)
#else
#  define AALOG(args...) \
    do {                 \
    } while (0)
#endif

#define FORWARD_ACTION_TO_ACCESSIBLE(funcname, ...)         \
  if (RootAccessibleWrap* rootAcc = GetRoot()) {            \
    AccessibleWrap* acc = rootAcc->FindAccessibleById(aID); \
    if (!acc) {                                             \
      return;                                               \
    }                                                       \
                                                            \
    acc->funcname(__VA_ARGS__);                             \
  }

template <>
const char nsWindow::NativePtr<mozilla::a11y::SessionAccessibility>::sName[] =
    "SessionAccessibility";

using namespace mozilla::a11y;

class Settings final
    : public mozilla::java::SessionAccessibility::Settings::Natives<Settings> {
 public:
  static void ToggleNativeAccessibility(bool aEnable) {
    if (aEnable) {
      GetOrCreateAccService();
    } else {
      MaybeShutdownAccService(nsAccessibilityService::ePlatformAPI);
    }
  }
};

void SessionAccessibility::SetAttached(bool aAttached,
                                       already_AddRefed<Runnable> aRunnable) {
  if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
    uiThread->Dispatch(NS_NewRunnableFunction(
        "SessionAccessibility::Attach",
        [aAttached,
         sa = java::SessionAccessibility::NativeProvider::GlobalRef(
             mSessionAccessibility),
         runnable = RefPtr<Runnable>(aRunnable)] {
          sa->SetAttached(aAttached);
          if (runnable) {
            runnable->Run();
          }
        }));
  }
}

void SessionAccessibility::Init() {
  java::SessionAccessibility::NativeProvider::Natives<
      SessionAccessibility>::Init();
  Settings::Init();
}

mozilla::jni::Object::LocalRef SessionAccessibility::GetNodeInfo(int32_t aID) {
  java::GeckoBundle::GlobalRef ret = nullptr;
  RefPtr<SessionAccessibility> self(this);
  nsAppShell::SyncRunEvent([this, self, aID, &ret] {
    if (RootAccessibleWrap* rootAcc = GetRoot()) {
      AccessibleWrap* acc = rootAcc->FindAccessibleById(aID);
      if (acc) {
        ret = acc->ToBundle();
      } else {
        AALOG("oops, nothing for %d", aID);
      }
    }
  });

  return mozilla::jni::Object::Ref::From(ret);
}

RootAccessibleWrap* SessionAccessibility::GetRoot() {
  if (!mWindow) {
    return nullptr;
  }

  return static_cast<RootAccessibleWrap*>(mWindow->GetRootAccessible());
}

void SessionAccessibility::SetText(int32_t aID, jni::String::Param aText) {
  FORWARD_ACTION_TO_ACCESSIBLE(SetTextContents, aText->ToString());
}

void SessionAccessibility::Click(int32_t aID) {
  FORWARD_ACTION_TO_ACCESSIBLE(DoAction, 0);
}

void SessionAccessibility::Pivot(int32_t aID, int32_t aGranularity,
                                 bool aForward, bool aInclusive) {
  FORWARD_ACTION_TO_ACCESSIBLE(Pivot, aGranularity, aForward, aInclusive);
}

void SessionAccessibility::ExploreByTouch(int32_t aID, float aX, float aY) {
  FORWARD_ACTION_TO_ACCESSIBLE(ExploreByTouch, aX, aY);
}

void SessionAccessibility::NavigateText(int32_t aID, int32_t aGranularity,
                                        int32_t aStartOffset,
                                        int32_t aEndOffset, bool aForward,
                                        bool aSelect) {
  FORWARD_ACTION_TO_ACCESSIBLE(NavigateText, aGranularity, aStartOffset,
                               aEndOffset, aForward, aSelect);
}

void SessionAccessibility::SetSelection(int32_t aID, int32_t aStart,
                                        int32_t aEnd) {
  FORWARD_ACTION_TO_ACCESSIBLE(SetSelection, aStart, aEnd);
}

SessionAccessibility* SessionAccessibility::GetInstanceFor(
    ProxyAccessible* aAccessible) {
  auto tab =
      static_cast<dom::BrowserParent*>(aAccessible->Document()->Manager());
  dom::Element* frame = tab->GetOwnerElement();
  MOZ_ASSERT(frame);
  if (!frame) {
    return nullptr;
  }

  Accessible* chromeDoc = GetExistingDocAccessible(frame->OwnerDoc());
  return chromeDoc ? GetInstanceFor(chromeDoc) : nullptr;
}

SessionAccessibility* SessionAccessibility::GetInstanceFor(
    Accessible* aAccessible) {
  RootAccessible* rootAcc = aAccessible->RootAccessible();
  nsViewManager* vm = rootAcc->PresShellPtr()->GetViewManager();
  if (!vm) {
    return nullptr;
  }

  nsCOMPtr<nsIWidget> rootWidget;
  vm->GetRootWidget(getter_AddRefs(rootWidget));
  // `rootWidget` can be one of several types. Here we make sure it is an
  // android nsWindow.
  if (RefPtr<nsWindow> window = nsWindow::From(rootWidget)) {
    return window->GetSessionAccessibility();
  }

  return nullptr;
}

void SessionAccessibility::SendAccessibilityFocusedEvent(
    AccessibleWrap* aAccessible) {
  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_ACCESSIBILITY_FOCUSED,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
  aAccessible->ScrollTo(nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
}

void SessionAccessibility::SendHoverEnterEvent(AccessibleWrap* aAccessible) {
  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_HOVER_ENTER,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void SessionAccessibility::SendFocusEvent(AccessibleWrap* aAccessible) {
  // Suppress focus events from about:blank pages.
  // This is important for tests.
  if (aAccessible->IsDoc() && aAccessible->ChildCount() == 0) {
    return;
  }

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_FOCUSED,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void SessionAccessibility::SendScrollingEvent(AccessibleWrap* aAccessible,
                                              int32_t aScrollX,
                                              int32_t aScrollY,
                                              int32_t aMaxScrollX,
                                              int32_t aMaxScrollY) {
  int32_t virtualViewId = aAccessible->VirtualViewID();

  if (virtualViewId != AccessibleWrap::kNoID) {
    // XXX: Support scrolling in subframes
    return;
  }

  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "scrollX", java::sdk::Integer::ValueOf(aScrollX));
  GECKOBUNDLE_PUT(eventInfo, "scrollY", java::sdk::Integer::ValueOf(aScrollY));
  GECKOBUNDLE_PUT(eventInfo, "maxScrollX",
                  java::sdk::Integer::ValueOf(aMaxScrollX));
  GECKOBUNDLE_PUT(eventInfo, "maxScrollY",
                  java::sdk::Integer::ValueOf(aMaxScrollY));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_SCROLLED, virtualViewId,
      aAccessible->AndroidClass(), eventInfo);
}

void SessionAccessibility::SendWindowContentChangedEvent() {
  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_WINDOW_CONTENT_CHANGED,
      AccessibleWrap::kNoID, java::SessionAccessibility::CLASSNAME_WEBVIEW,
      nullptr);
}

void SessionAccessibility::SendWindowStateChangedEvent(
    AccessibleWrap* aAccessible) {
  // Suppress window state changed events from about:blank pages.
  // This is important for tests.
  if (aAccessible->IsDoc() && aAccessible->ChildCount() == 0) {
    return;
  }

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_WINDOW_STATE_CHANGED,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void SessionAccessibility::SendTextSelectionChangedEvent(
    AccessibleWrap* aAccessible, int32_t aCaretOffset) {
  int32_t fromIndex = aCaretOffset;
  int32_t startSel = -1;
  int32_t endSel = -1;
  if (aAccessible->GetSelectionBounds(&startSel, &endSel)) {
    fromIndex = startSel == aCaretOffset ? endSel : startSel;
  }

  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "fromIndex",
                  java::sdk::Integer::ValueOf(fromIndex));
  GECKOBUNDLE_PUT(eventInfo, "toIndex",
                  java::sdk::Integer::ValueOf(aCaretOffset));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_TEXT_SELECTION_CHANGED,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void SessionAccessibility::SendTextChangedEvent(AccessibleWrap* aAccessible,
                                                const nsString& aStr,
                                                int32_t aStart, uint32_t aLen,
                                                bool aIsInsert,
                                                bool aFromUser) {
  if (!aFromUser) {
    // Only dispatch text change events from users, for now.
    return;
  }

  nsAutoString text;
  aAccessible->GetTextContents(text);
  nsAutoString beforeText(text);
  if (aIsInsert) {
    beforeText.Cut(aStart, aLen);
  } else {
    beforeText.Insert(aStr, aStart);
  }

  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "text", jni::StringParam(text));
  GECKOBUNDLE_PUT(eventInfo, "beforeText", jni::StringParam(beforeText));
  GECKOBUNDLE_PUT(eventInfo, "addedCount",
                  java::sdk::Integer::ValueOf(aIsInsert ? aLen : 0));
  GECKOBUNDLE_PUT(eventInfo, "removedCount",
                  java::sdk::Integer::ValueOf(aIsInsert ? 0 : aLen));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_TEXT_CHANGED,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void SessionAccessibility::SendTextTraversedEvent(AccessibleWrap* aAccessible,
                                                  int32_t aStartOffset,
                                                  int32_t aEndOffset) {
  nsAutoString text;
  aAccessible->GetTextContents(text);

  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "text", jni::StringParam(text));
  GECKOBUNDLE_PUT(eventInfo, "fromIndex",
                  java::sdk::Integer::ValueOf(aStartOffset));
  GECKOBUNDLE_PUT(eventInfo, "toIndex",
                  java::sdk::Integer::ValueOf(aEndOffset));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::
          TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void SessionAccessibility::SendClickedEvent(AccessibleWrap* aAccessible,
                                            bool aChecked) {
  GECKOBUNDLE_START(eventInfo);
  // Boolean::FALSE/TRUE gets clobbered by a macro, so ugh.
  GECKOBUNDLE_PUT(eventInfo, "checked",
                  java::sdk::Integer::ValueOf(aChecked ? 1 : 0));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_CLICKED,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void SessionAccessibility::SendSelectedEvent(AccessibleWrap* aAccessible,
                                             bool aSelected) {
  GECKOBUNDLE_START(eventInfo);
  // Boolean::FALSE/TRUE gets clobbered by a macro, so ugh.
  GECKOBUNDLE_PUT(eventInfo, "selected",
                  java::sdk::Integer::ValueOf(aSelected ? 1 : 0));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_VIEW_SELECTED,
      aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void SessionAccessibility::SendAnnouncementEvent(AccessibleWrap* aAccessible,
                                                 const nsString& aAnnouncement,
                                                 uint16_t aPriority) {
  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "text", jni::StringParam(aAnnouncement));
  GECKOBUNDLE_FINISH(eventInfo);

  // Announcements should have the root as their source, so we ignore the
  // accessible of the event.
  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_ANNOUNCEMENT, AccessibleWrap::kNoID,
      java::SessionAccessibility::CLASSNAME_WEBVIEW, eventInfo);
}

void SessionAccessibility::ReplaceViewportCache(
    const nsTArray<AccessibleWrap*>& aAccessibles,
    const nsTArray<BatchData>& aData) {
  auto infos = jni::ObjectArray::New<java::GeckoBundle>(aAccessibles.Length());
  for (size_t i = 0; i < aAccessibles.Length(); i++) {
    AccessibleWrap* acc = aAccessibles.ElementAt(i);
    if (!acc) {
      MOZ_ASSERT_UNREACHABLE("Updated accessible is gone.");
      continue;
    }

    if (aData.Length() == aAccessibles.Length()) {
      const BatchData& data = aData.ElementAt(i);
      auto bundle = acc->ToBundle(
          data.State(), data.Bounds(), data.ActionCount(), data.Name(),
          data.TextValue(), data.DOMNodeID(), data.Description());
      infos->SetElement(i, bundle);
    } else {
      infos->SetElement(i, acc->ToBundle(true));
    }
  }

  mSessionAccessibility->ReplaceViewportCache(infos);
  SendWindowContentChangedEvent();
}

void SessionAccessibility::ReplaceFocusPathCache(
    const nsTArray<AccessibleWrap*>& aAccessibles,
    const nsTArray<BatchData>& aData) {
  auto infos = jni::ObjectArray::New<java::GeckoBundle>(aAccessibles.Length());
  for (size_t i = 0; i < aAccessibles.Length(); i++) {
    AccessibleWrap* acc = aAccessibles.ElementAt(i);
    if (!acc) {
      MOZ_ASSERT_UNREACHABLE("Updated accessible is gone.");
      continue;
    }

    if (aData.Length() == aAccessibles.Length()) {
      const BatchData& data = aData.ElementAt(i);
      nsCOMPtr<nsIPersistentProperties> props =
          AccessibleWrap::AttributeArrayToProperties(data.Attributes());
      auto bundle =
          acc->ToBundle(data.State(), data.Bounds(), data.ActionCount(),
                        data.Name(), data.TextValue(), data.DOMNodeID(),
                        data.Description(), data.CurValue(), data.MinValue(),
                        data.MaxValue(), data.Step(), props);
      infos->SetElement(i, bundle);
    } else {
      infos->SetElement(i, acc->ToBundle());
    }
  }

  mSessionAccessibility->ReplaceFocusPathCache(infos);
}

void SessionAccessibility::UpdateCachedBounds(
    const nsTArray<AccessibleWrap*>& aAccessibles,
    const nsTArray<BatchData>& aData) {
  auto infos = jni::ObjectArray::New<java::GeckoBundle>(aAccessibles.Length());
  for (size_t i = 0; i < aAccessibles.Length(); i++) {
    AccessibleWrap* acc = aAccessibles.ElementAt(i);
    if (!acc) {
      MOZ_ASSERT_UNREACHABLE("Updated accessible is gone.");
      continue;
    }

    if (aData.Length() == aAccessibles.Length()) {
      const BatchData& data = aData.ElementAt(i);
      auto bundle = acc->ToBundle(
          data.State(), data.Bounds(), data.ActionCount(), data.Name(),
          data.TextValue(), data.DOMNodeID(), data.Description());
      infos->SetElement(i, bundle);
    } else {
      infos->SetElement(i, acc->ToBundle(true));
    }
  }

  mSessionAccessibility->UpdateCachedBounds(infos);
  SendWindowContentChangedEvent();
}

#undef FORWARD_ACTION_TO_ACCESSIBLE