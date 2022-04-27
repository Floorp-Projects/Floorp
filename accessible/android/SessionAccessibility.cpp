/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionAccessibility.h"
#include "LocalAccessible-inl.h"
#include "AndroidUiThread.h"
#include "DocAccessibleParent.h"
#include "IDSet.h"
#include "nsThreadUtils.h"
#include "AccAttributes.h"
#include "AccessibilityEvent.h"
#include "HyperTextAccessible.h"
#include "JavaBuiltins.h"
#include "RootAccessibleWrap.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsViewManager.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/jni/GeckoBundleUtils.h"
#include "mozilla/widget/GeckoViewSupport.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/StaticPrefs_accessibility.h"

#ifdef DEBUG
#  include <android/log.h>
#  define AALOG(args...) \
    __android_log_print(ANDROID_LOG_INFO, "GeckoAccessibilityNative", ##args)
#else
#  define AALOG(args...) \
    do {                 \
    } while (0)
#endif

#define FORWARD_ACTION_TO_ACCESSIBLE(funcname, ...)   \
  if (AccessibleWrap* acc = GetAccessibleByID(aID)) { \
    acc->funcname(__VA_ARGS__);                       \
  }

using namespace mozilla::a11y;

// IDs should be a positive 32bit integer.
IDSet sIDSet(31UL);

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

SessionAccessibility::SessionAccessibility(
    jni::NativeWeakPtr<widget::GeckoViewSupport> aWindow,
    java::SessionAccessibility::NativeProvider::Param aSessionAccessibility)
    : mWindow(aWindow), mSessionAccessibility(aSessionAccessibility) {
  SetAttached(true, nullptr);
}

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

bool SessionAccessibility::IsCacheEnabled() {
  return StaticPrefs::accessibility_cache_enabled_AtStartup();
}

mozilla::jni::Object::LocalRef SessionAccessibility::GetNodeInfo(int32_t aID) {
  java::GeckoBundle::GlobalRef ret = nullptr;
  RefPtr<SessionAccessibility> self(this);
  nsAppShell::SyncRunEvent([this, self, aID, &ret] {
    if (AccessibleWrap* acc = GetAccessibleByID(aID)) {
      ret = ToBundle(acc);
    } else {
      AALOG("oops, nothing for %d", aID);
    }
  });

  return mozilla::jni::Object::Ref::From(ret);
}

void SessionAccessibility::SetText(int32_t aID, jni::String::Param aText) {
  FORWARD_ACTION_TO_ACCESSIBLE(SetTextContents, aText->ToString());
}

void SessionAccessibility::Click(int32_t aID) {
  FORWARD_ACTION_TO_ACCESSIBLE(DoAction, 0);
}

bool SessionAccessibility::CachedPivot(int32_t aID, int32_t aGranularity,
                                       bool aForward, bool aInclusive) {
  RefPtr<SessionAccessibility> self(this);
  bool ret = false;
  nsAppShell::SyncRunEvent(
      [this, self, aID, aGranularity, aForward, aInclusive, &ret] {
        if (AccessibleWrap* acc = GetAccessibleByID(aID)) {
          ret = acc->PivotTo(aGranularity, aForward, aInclusive);
        }
      });

  return ret;
}

void SessionAccessibility::Pivot(int32_t aID, int32_t aGranularity,
                                 bool aForward, bool aInclusive) {
  FORWARD_ACTION_TO_ACCESSIBLE(PivotTo, aGranularity, aForward, aInclusive);
}

void SessionAccessibility::ExploreByTouch(int32_t aID, float aX, float aY) {
  auto gvAccessor(mWindow.Access());
  if (gvAccessor) {
    if (nsWindow* gkWindow = gvAccessor->GetNsWindow()) {
      WidgetMouseEvent hittest(true, eMouseExploreByTouch, gkWindow,
                               WidgetMouseEvent::eReal);
      hittest.mRefPoint = LayoutDeviceIntPoint::Floor(aX, aY);
      hittest.mInputSource = dom::MouseEvent_Binding::MOZ_SOURCE_TOUCH;
      hittest.mFlags.mOnlyChromeDispatch = true;
      gkWindow->DispatchInputEvent(&hittest);
    }
  }
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

void SessionAccessibility::Cut(int32_t aID) {
  FORWARD_ACTION_TO_ACCESSIBLE(Cut);
}

void SessionAccessibility::Copy(int32_t aID) {
  FORWARD_ACTION_TO_ACCESSIBLE(Copy);
}

void SessionAccessibility::Paste(int32_t aID) {
  FORWARD_ACTION_TO_ACCESSIBLE(Paste);
}

#undef FORWARD_ACTION_TO_ACCESSIBLE

RefPtr<SessionAccessibility> SessionAccessibility::GetInstanceFor(
    Accessible* aAccessible) {
  PresShell* presShell = nullptr;
  if (LocalAccessible* localAcc = aAccessible->AsLocal()) {
    if (localAcc->IsProxy()) {
      // XXX: Will go away when removing RemoteAccessibleWrapper
      return GetInstanceFor(localAcc->Proxy());
    }
    DocAccessible* doc = localAcc->Document();
    if (doc && !doc->HasShutdown() &&
        doc->DocumentNode()->IsContentDocument()) {
      // Only content accessibles should have an associated SessionAccessible.
      presShell = doc->PresShellPtr();
    }
  } else {
    dom::CanonicalBrowsingContext* cbc =
        static_cast<dom::BrowserParent*>(
            aAccessible->AsRemote()->Document()->Manager())
            ->GetBrowsingContext()
            ->Top();
    dom::BrowserParent* bp = cbc->GetBrowserParent();
    if (!bp) {
      bp = static_cast<dom::BrowserParent*>(
          aAccessible->AsRemote()->Document()->Manager());
    }
    nsPresContext* presContext =
        bp->GetOwnerElement()->OwnerDoc()->GetPresContext();
    if (presContext) {
      presShell = presContext->PresShell();
    }
  }

  if (!presShell) {
    return nullptr;
  }

  nsViewManager* vm = presShell->GetViewManager();
  if (!vm) {
    return nullptr;
  }

  nsCOMPtr<nsIWidget> rootWidget = vm->GetRootWidget();
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

  if (virtualViewId != kNoID) {
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
  SendWindowContentChangedEvent();
}

void SessionAccessibility::SendWindowContentChangedEvent() {
  mSessionAccessibility->SendEvent(
      java::sdk::AccessibilityEvent::TYPE_WINDOW_CONTENT_CHANGED, kNoID,
      java::SessionAccessibility::CLASSNAME_WEBVIEW, nullptr);
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
                                            uint32_t aFlags) {
  GECKOBUNDLE_START(eventInfo);
  GECKOBUNDLE_PUT(eventInfo, "flags", java::sdk::Integer::ValueOf(aFlags));
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
      java::sdk::AccessibilityEvent::TYPE_ANNOUNCEMENT, kNoID,
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
      auto bundle = ToBundle(acc, data.State(), data.Bounds(),
                             data.ActionCount(), data.Name(), data.TextValue(),
                             data.DOMNodeID(), data.Description());
      infos->SetElement(i, bundle);
    } else {
      infos->SetElement(i, ToBundle(acc, true));
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
      auto bundle =
          ToBundle(acc, data.State(), data.Bounds(), data.ActionCount(),
                   data.Name(), data.TextValue(), data.DOMNodeID(),
                   data.Description(), data.CurValue(), data.MinValue(),
                   data.MaxValue(), data.Step(), data.Attributes());
      infos->SetElement(i, bundle);
    } else {
      infos->SetElement(i, ToBundle(acc));
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
      auto bundle = ToBundle(acc, data.State(), data.Bounds(),
                             data.ActionCount(), data.Name(), data.TextValue(),
                             data.DOMNodeID(), data.Description());
      infos->SetElement(i, bundle);
    } else {
      infos->SetElement(i, ToBundle(acc, true));
    }
  }

  mSessionAccessibility->UpdateCachedBounds(infos);
}

void SessionAccessibility::UpdateAccessibleFocusBoundaries(
    AccessibleWrap* aFirst, AccessibleWrap* aLast) {
  mSessionAccessibility->UpdateAccessibleFocusBoundaries(
      aFirst ? aFirst->VirtualViewID() : kNoID,
      aLast ? aLast->VirtualViewID() : kNoID);
}

mozilla::java::GeckoBundle::LocalRef SessionAccessibility::ToBundle(
    AccessibleWrap* aAccessible, bool aSmall) {
  nsAutoString name;
  aAccessible->Name(name);
  nsAutoString textValue;
  aAccessible->Value(textValue);
  nsAutoString nodeID;
  aAccessible->WrapperDOMNodeID(nodeID);
  nsAutoString description;
  aAccessible->Description(description);
  uint64_t state = aAccessible->State();
  LayoutDeviceIntRect bounds = aAccessible->Bounds();
  uint8_t actionCount = aAccessible->ActionCount();

  if (aSmall) {
    return ToBundle(aAccessible, state, bounds, actionCount, name, textValue,
                    nodeID, description);
  }

  double curValue = UnspecifiedNaN<double>();
  double minValue = UnspecifiedNaN<double>();
  double maxValue = UnspecifiedNaN<double>();
  double step = UnspecifiedNaN<double>();
  aAccessible->WrapperRangeInfo(&curValue, &minValue, &maxValue, &step);

  RefPtr<AccAttributes> attributes = aAccessible->Attributes();

  return ToBundle(aAccessible, state, bounds, actionCount, name, textValue,
                  nodeID, description, curValue, minValue, maxValue, step,
                  attributes);
}

mozilla::java::GeckoBundle::LocalRef SessionAccessibility::ToBundle(
    AccessibleWrap* aAccessible, const uint64_t aState,
    const LayoutDeviceIntRect& aBounds, const uint8_t aActionCount,
    const nsString& aName, const nsString& aTextValue,
    const nsString& aDOMNodeID, const nsString& aDescription,
    const double& aCurVal, const double& aMinVal, const double& aMaxVal,
    const double& aStep, AccAttributes* aAttributes) {
  int32_t virtualViewID = aAccessible->VirtualViewID();
  GECKOBUNDLE_START(nodeInfo);
  GECKOBUNDLE_PUT(nodeInfo, "id", java::sdk::Integer::ValueOf(virtualViewID));

  AccessibleWrap* parent = aAccessible->WrapperParent();
  GECKOBUNDLE_PUT(
      nodeInfo, "parentId",
      java::sdk::Integer::ValueOf(parent ? parent->VirtualViewID() : 0));

  role role = aAccessible->WrapperRole();
  if (role == roles::LINK && !(aState & states::LINKED)) {
    // A link without the linked state (<a> with no href) shouldn't be presented
    // as a link.
    role = roles::TEXT;
  }

  uint32_t flags = AccessibleWrap::GetFlags(role, aState, aActionCount);
  GECKOBUNDLE_PUT(nodeInfo, "flags", java::sdk::Integer::ValueOf(flags));
  GECKOBUNDLE_PUT(nodeInfo, "className",
                  java::sdk::Integer::ValueOf(aAccessible->AndroidClass()));

  nsAutoString hint;
  if (aState & states::EDITABLE) {
    // An editable field's name is populated in the hint.
    hint.Assign(aName);
    GECKOBUNDLE_PUT(nodeInfo, "text", jni::StringParam(aTextValue));
  } else {
    if (role == roles::LINK || role == roles::HEADING) {
      GECKOBUNDLE_PUT(nodeInfo, "description", jni::StringParam(aName));
    } else {
      GECKOBUNDLE_PUT(nodeInfo, "text", jni::StringParam(aName));
    }
  }

  if (!aDescription.IsEmpty()) {
    if (!hint.IsEmpty()) {
      // If this is an editable, the description is concatenated with a
      // whitespace directly after the name.
      hint.AppendLiteral(" ");
    }
    hint.Append(aDescription);
  }

  if ((aState & states::REQUIRED) != 0) {
    nsAutoString requiredString;
    if (LocalizeString("stateRequired", requiredString)) {
      if (!hint.IsEmpty()) {
        // If the hint is non-empty, concatenate with a comma for a brief pause.
        hint.AppendLiteral(", ");
      }
      hint.Append(requiredString);
    }
  }

  if (!hint.IsEmpty()) {
    GECKOBUNDLE_PUT(nodeInfo, "hint", jni::StringParam(hint));
  }

  nsAutoString geckoRole;
  nsAutoString roleDescription;
  if (virtualViewID != kNoID) {
    AccessibleWrap::GetRoleDescription(role, aAttributes, geckoRole,
                                       roleDescription);
  }

  GECKOBUNDLE_PUT(nodeInfo, "roleDescription",
                  jni::StringParam(roleDescription));
  GECKOBUNDLE_PUT(nodeInfo, "geckoRole", jni::StringParam(geckoRole));

  if (!aDOMNodeID.IsEmpty()) {
    GECKOBUNDLE_PUT(nodeInfo, "viewIdResourceName",
                    jni::StringParam(aDOMNodeID));
  }

  const int32_t data[4] = {aBounds.x, aBounds.y, aBounds.x + aBounds.width,
                           aBounds.y + aBounds.height};
  GECKOBUNDLE_PUT(nodeInfo, "bounds", jni::IntArray::New(data, 4));

  if (aAccessible->HasNumericValue()) {
    GECKOBUNDLE_START(rangeInfo);
    if (aMaxVal == 1 && aMinVal == 0) {
      GECKOBUNDLE_PUT(rangeInfo, "type",
                      java::sdk::Integer::ValueOf(2));  // percent
    } else if (std::round(aStep) != aStep) {
      GECKOBUNDLE_PUT(rangeInfo, "type",
                      java::sdk::Integer::ValueOf(1));  // float
    } else {
      GECKOBUNDLE_PUT(rangeInfo, "type",
                      java::sdk::Integer::ValueOf(0));  // integer
    }

    if (!IsNaN(aCurVal)) {
      GECKOBUNDLE_PUT(rangeInfo, "current", java::sdk::Double::New(aCurVal));
    }
    if (!IsNaN(aMinVal)) {
      GECKOBUNDLE_PUT(rangeInfo, "min", java::sdk::Double::New(aMinVal));
    }
    if (!IsNaN(aMaxVal)) {
      GECKOBUNDLE_PUT(rangeInfo, "max", java::sdk::Double::New(aMaxVal));
    }

    GECKOBUNDLE_FINISH(rangeInfo);
    GECKOBUNDLE_PUT(nodeInfo, "rangeInfo", rangeInfo);
  }

  if (aAttributes) {
    nsString inputTypeAttr;
    aAttributes->GetAttribute(nsGkAtoms::textInputType, inputTypeAttr);
    int32_t inputType = AccessibleWrap::GetInputType(inputTypeAttr);
    if (inputType) {
      GECKOBUNDLE_PUT(nodeInfo, "inputType",
                      java::sdk::Integer::ValueOf(inputType));
    }

    Maybe<int32_t> rowIndex =
        aAttributes->GetAttribute<int32_t>(nsGkAtoms::posinset);
    if (rowIndex) {
      GECKOBUNDLE_START(collectionItemInfo);
      GECKOBUNDLE_PUT(collectionItemInfo, "rowIndex",
                      java::sdk::Integer::ValueOf(*rowIndex));
      GECKOBUNDLE_PUT(collectionItemInfo, "columnIndex",
                      java::sdk::Integer::ValueOf(0));
      GECKOBUNDLE_PUT(collectionItemInfo, "rowSpan",
                      java::sdk::Integer::ValueOf(1));
      GECKOBUNDLE_PUT(collectionItemInfo, "columnSpan",
                      java::sdk::Integer::ValueOf(1));
      GECKOBUNDLE_FINISH(collectionItemInfo);

      GECKOBUNDLE_PUT(nodeInfo, "collectionItemInfo", collectionItemInfo);
    }

    Maybe<int32_t> rowCount =
        aAttributes->GetAttribute<int32_t>(nsGkAtoms::child_item_count);
    if (rowCount) {
      GECKOBUNDLE_START(collectionInfo);
      GECKOBUNDLE_PUT(collectionInfo, "rowCount",
                      java::sdk::Integer::ValueOf(*rowCount));
      GECKOBUNDLE_PUT(collectionInfo, "columnCount",
                      java::sdk::Integer::ValueOf(1));

      if (aAttributes->HasAttribute(nsGkAtoms::tree)) {
        GECKOBUNDLE_PUT(collectionInfo, "isHierarchical",
                        java::sdk::Boolean::TRUE());
      }

      if (aAccessible->IsSelect()) {
        int32_t selectionMode = (aState & states::MULTISELECTABLE) ? 2 : 1;
        GECKOBUNDLE_PUT(collectionInfo, "selectionMode",
                        java::sdk::Integer::ValueOf(selectionMode));
      }

      GECKOBUNDLE_FINISH(collectionInfo);
      GECKOBUNDLE_PUT(nodeInfo, "collectionInfo", collectionInfo);
    }
  }

  bool mustPrune = aAccessible->IsProxy()
                       ? nsAccUtils::MustPrune(aAccessible->Proxy())
                       : nsAccUtils::MustPrune(aAccessible);
  if (!mustPrune) {
    auto childCount = aAccessible->ChildCount();
    nsTArray<int32_t> children(childCount);
    for (uint32_t i = 0; i < childCount; i++) {
      auto child = static_cast<AccessibleWrap*>(aAccessible->LocalChildAt(i));
      children.AppendElement(child->VirtualViewID());
    }

    GECKOBUNDLE_PUT(nodeInfo, "children",
                    jni::IntArray::New(children.Elements(), children.Length()));
  }

  GECKOBUNDLE_FINISH(nodeInfo);

  return nodeInfo;
}

void SessionAccessibility::RegisterAccessible(Accessible* aAccessible) {
  if (IPCAccessibilityActive()) {
    // Don't register accessible in content process.
    return;
  }

  RefPtr<SessionAccessibility> sessionAcc = GetInstanceFor(aAccessible);
  if (!sessionAcc) {
    return;
  }

  bool isTopLevel = false;
  if (aAccessible->IsLocal() && aAccessible->IsDoc()) {
    DocAccessibleWrap* doc =
        static_cast<DocAccessibleWrap*>(aAccessible->AsLocal()->AsDoc());
    isTopLevel = doc->IsTopLevelContentDoc();
  } else if (aAccessible->IsRemote() && aAccessible->IsDoc()) {
    isTopLevel = aAccessible->AsRemote()->AsDoc()->IsTopLevel();
  }

  int32_t virtualViewID = kNoID;
  if (!isTopLevel) {
    // Don't use the special "unset" value (0).
    while ((virtualViewID = sIDSet.GetID()) == kUnsetID) {
    }
  }
  AccessibleWrap::SetVirtualViewID(aAccessible, virtualViewID);

  MOZ_ASSERT(
      !sessionAcc->mIDToAccessibleMap.IsEmpty() || virtualViewID == kNoID,
      "root (kNoID) accessible should be the first one added");
  MOZ_ASSERT(!sessionAcc->mIDToAccessibleMap.Contains(virtualViewID),
             "ID already registered");
  sessionAcc->mIDToAccessibleMap.InsertOrUpdate(virtualViewID, aAccessible);
}

void SessionAccessibility::UnregisterAccessible(Accessible* aAccessible) {
  if (IPCAccessibilityActive()) {
    // Don't unregister accessible in content process.
    return;
  }

  int32_t virtualViewID = AccessibleWrap::GetVirtualViewID(aAccessible);
  if (virtualViewID == kUnsetID) {
    return;
  }

  RefPtr<SessionAccessibility> sessionAcc = GetInstanceFor(aAccessible);
  if (sessionAcc) {
    MOZ_ASSERT(sessionAcc->mIDToAccessibleMap.Contains(virtualViewID),
               "Unregistering unregistered accessible");
    MOZ_ASSERT(
        virtualViewID == kNoID || sessionAcc->mIDToAccessibleMap.Count() > 1,
        "Root accessible should be the last one removed");
    sessionAcc->mIDToAccessibleMap.Remove(virtualViewID);
  }

  if (virtualViewID > kNoID) {
    sIDSet.ReleaseID(virtualViewID);
  }

  AccessibleWrap::SetVirtualViewID(aAccessible, kUnsetID);
}
