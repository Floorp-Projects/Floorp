/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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

#ifdef DEBUG
#include <android/log.h>
#define AALOG(args...)                                                         \
  __android_log_print(ANDROID_LOG_INFO, "GeckoAccessibilityNative", ##args)
#else
#define AALOG(args...)                                                         \
  do {                                                                         \
  } while (0)
#endif

template<>
const char nsWindow::NativePtr<mozilla::a11y::SessionAccessibility>::sName[] =
  "SessionAccessibility";

using namespace mozilla::a11y;

class Settings final
  : public mozilla::java::SessionAccessibility::Settings::Natives<Settings>
{
public:
  static void ToggleNativeAccessibility(bool aEnable)
  {
    if (aEnable) {
      GetOrCreateAccService();
    } else {
      MaybeShutdownAccService(nsAccessibilityService::ePlatformAPI);
    }
  }
};

void
SessionAccessibility::SetAttached(bool aAttached,
                                  already_AddRefed<Runnable> aRunnable)
{
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

void
SessionAccessibility::Init()
{
  java::SessionAccessibility::NativeProvider::Natives<
    SessionAccessibility>::Init();
  Settings::Init();
}

mozilla::jni::Object::LocalRef
SessionAccessibility::GetNodeInfo(int32_t aID)
{
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

RootAccessibleWrap*
SessionAccessibility::GetRoot()
{
  if (!mWindow) {
    return nullptr;
  }

  return static_cast<RootAccessibleWrap*>(mWindow->GetRootAccessible());
}

void
SessionAccessibility::SetText(int32_t aID, jni::String::Param aText)
{
  if (RootAccessibleWrap* rootAcc = GetRoot()) {
    AccessibleWrap* acc = rootAcc->FindAccessibleById(aID);
    if (!acc) {
      return;
    }

    acc->SetTextContents(aText->ToString());
  }
}

SessionAccessibility*
SessionAccessibility::GetInstanceFor(ProxyAccessible* aAccessible)
{
  Accessible* outerDoc = aAccessible->OuterDocOfRemoteBrowser();
  if (!outerDoc) {
    return nullptr;
  }

  return GetInstanceFor(outerDoc);
}

SessionAccessibility*
SessionAccessibility::GetInstanceFor(Accessible* aAccessible)
{
  RootAccessible* rootAcc = aAccessible->RootAccessible();
  nsIPresShell* shell = rootAcc->PresShell();
  nsViewManager* vm = shell->GetViewManager();
  if (!vm) {
    return nullptr;
  }

  nsCOMPtr<nsIWidget> rootWidget;
  vm->GetRootWidget(getter_AddRefs(rootWidget));
  // `rootWidget` can be one of several types. Here we make sure it is an
  // android nsWindow that implemented NS_NATIVE_WIDGET to return itself.
  if (rootWidget &&
      rootWidget->WindowType() == nsWindowType::eWindowType_toplevel &&
      rootWidget->GetNativeData(NS_NATIVE_WIDGET) == rootWidget) {
    return static_cast<nsWindow*>(rootWidget.get())->GetSessionAccessibility();
  }

  return nullptr;
}

void
SessionAccessibility::SendAccessibilityFocusedEvent(AccessibleWrap* aAccessible)
{
  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_ACCESSIBILITY_FOCUSED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
  aAccessible->ScrollTo(nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
}

void
SessionAccessibility::SendHoverEnterEvent(AccessibleWrap* aAccessible)
{
  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_HOVER_ENTER,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void
SessionAccessibility::SendFocusEvent(AccessibleWrap* aAccessible)
{
  // Suppress focus events from about:blank pages.
  // This is important for tests.
  if (aAccessible->IsDoc() && aAccessible->ChildCount() == 0) {
    return;
  }

  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_FOCUSED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void
SessionAccessibility::SendScrollingEvent(AccessibleWrap* aAccessible,
                                         int32_t aScrollX,
                                         int32_t aScrollY,
                                         int32_t aMaxScrollX,
                                         int32_t aMaxScrollY)
{
  int32_t virtualViewId = aAccessible->VirtualViewID();

  if (virtualViewId != AccessibleWrap::kNoID) {
    // XXX: Support scrolling in subframes
    return;
  }

  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "scrollX", java::sdk::Integer::ValueOf(aScrollX));
  GECKOBUNDLE_PUT(eventInfo, "scrollY", java::sdk::Integer::ValueOf(aScrollY));
  GECKOBUNDLE_PUT(eventInfo, "maxScrollX", java::sdk::Integer::ValueOf(aMaxScrollX));
  GECKOBUNDLE_PUT(eventInfo, "maxScrollY", java::sdk::Integer::ValueOf(aMaxScrollY));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_SCROLLED, virtualViewId,
    aAccessible->AndroidClass(), eventInfo);

  SendWindowContentChangedEvent(aAccessible);
}

void
SessionAccessibility::SendWindowContentChangedEvent(AccessibleWrap* aAccessible)
{
  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_WINDOW_CONTENT_CHANGED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void
SessionAccessibility::SendWindowStateChangedEvent(AccessibleWrap* aAccessible)
{
  // Suppress window state changed events from about:blank pages.
  // This is important for tests.
  if (aAccessible->IsDoc() && aAccessible->ChildCount() == 0) {
    return;
  }

  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_WINDOW_STATE_CHANGED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void
SessionAccessibility::SendTextSelectionChangedEvent(AccessibleWrap* aAccessible,
                                                    int32_t aCaretOffset)
{
  int32_t fromIndex = aCaretOffset;
  int32_t startSel = -1;
  int32_t endSel = -1;
  if (aAccessible->GetSelectionBounds(&startSel, &endSel)) {
    fromIndex = startSel == aCaretOffset ? endSel : startSel;
  }

  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "fromIndex", java::sdk::Integer::ValueOf(fromIndex));
  GECKOBUNDLE_PUT(eventInfo, "toIndex", java::sdk::Integer::ValueOf(aCaretOffset));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_TEXT_SELECTION_CHANGED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void
SessionAccessibility::SendTextChangedEvent(AccessibleWrap* aAccessible,
                                           const nsString& aStr,
                                           int32_t aStart,
                                           uint32_t aLen,
                                           bool aIsInsert,
                                           bool aFromUser)
{
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
  GECKOBUNDLE_PUT(eventInfo, "addedCount", java::sdk::Integer::ValueOf(aIsInsert ? aLen : 0));
  GECKOBUNDLE_PUT(eventInfo, "removedCount", java::sdk::Integer::ValueOf(aIsInsert ? 0 : aLen));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_TEXT_CHANGED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void
SessionAccessibility::SendTextTraversedEvent(AccessibleWrap* aAccessible,
                                             int32_t aStartOffset,
                                             int32_t aEndOffset)
{
  nsAutoString text;
  aAccessible->GetTextContents(text);

  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "text", jni::StringParam(text));
  GECKOBUNDLE_PUT(eventInfo, "fromIndex", java::sdk::Integer::ValueOf(aStartOffset));
  GECKOBUNDLE_PUT(eventInfo, "toIndex", java::sdk::Integer::ValueOf(aEndOffset));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::
      TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void
SessionAccessibility::SendClickedEvent(AccessibleWrap* aAccessible, bool aChecked)
{
  GECKOBUNDLE_START(eventInfo);
  // Boolean::FALSE/TRUE gets clobbered by a macro, so ugh.
  GECKOBUNDLE_PUT(eventInfo, "checked", java::sdk::Integer::ValueOf(aChecked ? 1 : 0));
  GECKOBUNDLE_FINISH(eventInfo);

  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_CLICKED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), eventInfo);
}

void
SessionAccessibility::SendSelectedEvent(AccessibleWrap* aAccessible)
{
  mSessionAccessibility->SendEvent(
    java::sdk::AccessibilityEvent::TYPE_VIEW_SELECTED,
    aAccessible->VirtualViewID(), aAccessible->AndroidClass(), nullptr);
}

void
SessionAccessibility::ReplaceViewportCache(const nsTArray<AccessibleWrap*>& aAccessibles,
                                          const nsTArray<BatchData>& aData)
{
  auto infos = jni::ObjectArray::New<java::GeckoBundle>(aAccessibles.Length());
  for (size_t i = 0; i < aAccessibles.Length(); i++) {
    AccessibleWrap* acc = aAccessibles.ElementAt(i);
    if (aData.Length() == aAccessibles.Length()) {
      const BatchData& data = aData.ElementAt(i);
      infos->SetElement(i, acc->ToSmallBundle(data.State(), data.Bounds()));
    } else {
      infos->SetElement(i, acc->ToSmallBundle());
    }
  }

  mSessionAccessibility->ReplaceViewportCache(infos);
}
