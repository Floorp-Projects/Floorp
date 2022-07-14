/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "JavaBuiltins.h"
#include "LocalAccessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "AccAttributes.h"
#include "AccEvent.h"
#include "AndroidInputType.h"
#include "DocAccessibleWrap.h"
#include "SessionAccessibility.h"
#include "TextLeafAccessible.h"
#include "TraversalRule.h"
#include "Pivot.h"
#include "Platform.h"
#include "nsAccessibilityService.h"
#include "nsEventShell.h"
#include "nsIAccessibleAnnouncementEvent.h"
#include "nsAccUtils.h"
#include "nsTextEquivUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "RootAccessible.h"

#include "mozilla/a11y/PDocAccessibleChild.h"
#include "mozilla/jni/GeckoBundleUtils.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/a11y/DocAccessibleParent.h"

// icu TRUE conflicting with java::sdk::Boolean::TRUE()
// https://searchfox.org/mozilla-central/rev/ce02064d8afc8673cef83c92896ee873bd35e7ae/intl/icu/source/common/unicode/umachine.h#265
// https://searchfox.org/mozilla-central/source/__GENERATED__/widget/android/bindings/JavaBuiltins.h#78
#ifdef TRUE
#  undef TRUE
#endif

using namespace mozilla::a11y;

//-----------------------------------------------------
// construction
//-----------------------------------------------------
AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
    : LocalAccessible(aContent, aDoc), mID(SessionAccessibility::kUnsetID) {
  if (!IPCAccessibilityActive()) {
    MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
    SessionAccessibility::RegisterAccessible(this);
  }
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
AccessibleWrap::~AccessibleWrap() {}

nsresult AccessibleWrap::HandleAccEvent(AccEvent* aEvent) {
  auto accessible = static_cast<AccessibleWrap*>(aEvent->GetAccessible());
  NS_ENSURE_TRUE(accessible, NS_ERROR_FAILURE);
  DocAccessibleWrap* doc =
      static_cast<DocAccessibleWrap*>(accessible->Document());
  if (doc) {
    switch (aEvent->GetEventType()) {
      case nsIAccessibleEvent::EVENT_FOCUS: {
        if (DocAccessibleWrap* topContentDoc =
                doc->GetTopLevelContentDoc(accessible)) {
          topContentDoc->CacheFocusPath(accessible);
        }
        break;
      }
      case nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED: {
        AccVCChangeEvent* vcEvent = downcast_accEvent(aEvent);
        auto newPosition =
            static_cast<AccessibleWrap*>(vcEvent->NewAccessible());
        if (newPosition) {
          if (DocAccessibleWrap* topContentDoc =
                  doc->GetTopLevelContentDoc(accessible)) {
            topContentDoc->CacheFocusPath(newPosition);
          }
        }
        break;
      }
      case nsIAccessibleEvent::EVENT_REORDER: {
        if (DocAccessibleWrap* topContentDoc =
                doc->GetTopLevelContentDoc(accessible)) {
          topContentDoc->CacheViewport(true);
        }
        break;
      }
      case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED: {
        if (accessible != aEvent->Document() && !aEvent->IsFromUserInput()) {
          AccCaretMoveEvent* caretEvent = downcast_accEvent(aEvent);
          HyperTextAccessible* ht = AsHyperText();
          // Pivot to the caret's position if it has an expanded selection.
          // This is used mostly for find in page.
          if ((ht && ht->SelectionCount())) {
            DOMPoint point =
                AsHyperText()->OffsetToDOMPoint(caretEvent->GetCaretOffset());
            if (LocalAccessible* newPos =
                    doc->GetAccessibleOrContainer(point.node)) {
              static_cast<AccessibleWrap*>(newPos)->PivotTo(
                  java::SessionAccessibility::HTML_GRANULARITY_DEFAULT, true,
                  true);
            }
          }
        }
        break;
      }
      case nsIAccessibleEvent::EVENT_SCROLLING_START: {
        accessible->PivotTo(
            java::SessionAccessibility::HTML_GRANULARITY_DEFAULT, true, true);
        break;
      }
      default:
        break;
    }
  }

  nsresult rv = LocalAccessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  accessible->HandleLiveRegionEvent(aEvent);

  if (IPCAccessibilityActive()) {
    return NS_OK;
  }

  // The accessible can become defunct if we have an xpcom event listener
  // which decides it would be fun to change the DOM and flush layout.
  if (accessible->IsDefunct() || !accessible->IsBoundToParent()) {
    return NS_OK;
  }

  if (doc) {
    if (!doc->DocumentNode()->IsContentDocument()) {
      return NS_OK;
    }
  }

  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(accessible);
  if (!sessionAcc) {
    return NS_OK;
  }

  switch (aEvent->GetEventType()) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      sessionAcc->SendFocusEvent(accessible);
      break;
    case nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED: {
      AccVCChangeEvent* vcEvent = downcast_accEvent(aEvent);
      if (!vcEvent->IsFromUserInput()) {
        break;
      }

      RefPtr<AccessibleWrap> newPosition =
          static_cast<AccessibleWrap*>(vcEvent->NewAccessible());
      if (sessionAcc && newPosition) {
        if (vcEvent->Reason() == nsIAccessiblePivot::REASON_POINT) {
          sessionAcc->SendHoverEnterEvent(newPosition);
        } else if (vcEvent->BoundaryType() == nsIAccessiblePivot::NO_BOUNDARY) {
          sessionAcc->SendAccessibilityFocusedEvent(newPosition);
        }

        if (vcEvent->BoundaryType() != nsIAccessiblePivot::NO_BOUNDARY) {
          sessionAcc->SendTextTraversedEvent(
              newPosition, vcEvent->NewStartOffset(), vcEvent->NewEndOffset());
        }
      }
      break;
    }
    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED: {
      AccCaretMoveEvent* event = downcast_accEvent(aEvent);
      sessionAcc->SendTextSelectionChangedEvent(accessible,
                                                event->GetCaretOffset());
      break;
    }
    case nsIAccessibleEvent::EVENT_TEXT_INSERTED:
    case nsIAccessibleEvent::EVENT_TEXT_REMOVED: {
      AccTextChangeEvent* event = downcast_accEvent(aEvent);
      sessionAcc->SendTextChangedEvent(
          accessible, event->ModifiedText(), event->GetStartOffset(),
          event->GetLength(), event->IsTextInserted(),
          event->IsFromUserInput());
      break;
    }
    case nsIAccessibleEvent::EVENT_STATE_CHANGE: {
      AccStateChangeEvent* event = downcast_accEvent(aEvent);
      auto state = event->GetState();
      if (state & states::CHECKED) {
        sessionAcc->SendClickedEvent(
            accessible, java::SessionAccessibility::FLAG_CHECKABLE |
                            (event->IsStateEnabled()
                                 ? java::SessionAccessibility::FLAG_CHECKED
                                 : 0));
      }

      if (state & states::EXPANDED) {
        sessionAcc->SendClickedEvent(
            accessible, java::SessionAccessibility::FLAG_EXPANDABLE |
                            (event->IsStateEnabled()
                                 ? java::SessionAccessibility::FLAG_EXPANDED
                                 : 0));
      }

      if (state & states::SELECTED) {
        sessionAcc->SendSelectedEvent(accessible, event->IsStateEnabled());
      }

      if (state & states::BUSY) {
        sessionAcc->SendWindowStateChangedEvent(accessible);
      }
      break;
    }
    case nsIAccessibleEvent::EVENT_SCROLLING: {
      AccScrollingEvent* event = downcast_accEvent(aEvent);
      sessionAcc->SendScrollingEvent(accessible, event->ScrollX(),
                                     event->ScrollY(), event->MaxScrollX(),
                                     event->MaxScrollY());
      break;
    }
    case nsIAccessibleEvent::EVENT_ANNOUNCEMENT: {
      AccAnnouncementEvent* event = downcast_accEvent(aEvent);
      sessionAcc->SendAnnouncementEvent(accessible, event->Announcement(),
                                        event->Priority());
      break;
    }
    case nsIAccessibleEvent::EVENT_REORDER: {
      if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
        sessionAcc->SendWindowContentChangedEvent();
      }
      break;
    }
    default:
      break;
  }

  return NS_OK;
}

void AccessibleWrap::Shutdown() {
  if (!IPCAccessibilityActive()) {
    MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
    SessionAccessibility::UnregisterAccessible(this);
  }
  LocalAccessible::Shutdown();
}

bool AccessibleWrap::DoAction(uint8_t aIndex) const {
  if (ActionCount()) {
    return LocalAccessible::DoAction(aIndex);
  }

  if (mContent) {
    // We still simulate a click on an accessible even if there is no
    // known actions. For the sake of bad markup.
    DoCommand();
    return true;
  }

  return false;
}

Accessible* AccessibleWrap::DoPivot(Accessible* aAccessible,
                                    int32_t aGranularity, bool aForward,
                                    bool aInclusive) {
  a11y::Pivot pivot(nullptr);
  // Depending on the start accessible, the pivot rule will either traverse
  // local or remote accessibles exclusively.
  TraversalRule rule(aGranularity, aAccessible->IsLocal());
  Accessible* result = aForward ? pivot.Next(aAccessible, rule, aInclusive)
                                : pivot.Prev(aAccessible, rule, aInclusive);

  if (result && (result != aAccessible || aInclusive)) {
    return result;
  }

  return nullptr;
}

bool AccessibleWrap::PivotTo(int32_t aGranularity, bool aForward,
                             bool aInclusive) {
  Accessible* result = DoPivot(this, aGranularity, aForward, aInclusive);
  if (result) {
    MOZ_ASSERT(result->IsLocal());
    // Dispatch a virtual cursor change event that will be turned into an
    // android accessibility focused changed event in the parent.
    PivotMoveReason reason = aForward ? nsIAccessiblePivot::REASON_NEXT
                                      : nsIAccessiblePivot::REASON_PREV;
    LocalAccessible* localResult = result->AsLocal();
    RefPtr<AccEvent> event = new AccVCChangeEvent(
        localResult->Document(), this, -1, -1, localResult, -1, -1, reason,
        nsIAccessiblePivot::NO_BOUNDARY, eFromUserInput);
    nsEventShell::FireEvent(event);

    return true;
  }

  return false;
}

void AccessibleWrap::ExploreByTouch(float aX, float aY) {
  a11y::Pivot pivot(RootAccessible());
  TraversalRule rule;

  Accessible* maybeResult = pivot.AtPoint(aX, aY, rule);
  LocalAccessible* result = maybeResult ? maybeResult->AsLocal() : nullptr;

  if (result && result != this) {
    RefPtr<AccEvent> event =
        new AccVCChangeEvent(result->Document(), this, -1, -1, result, -1, -1,
                             nsIAccessiblePivot::REASON_POINT,
                             nsIAccessiblePivot::NO_BOUNDARY, eFromUserInput);
    nsEventShell::FireEvent(event);
  }
}

void AccessibleWrap::NavigateText(int32_t aGranularity, int32_t aStartOffset,
                                  int32_t aEndOffset, bool aForward,
                                  bool aSelect) {
  a11y::Pivot pivot(RootAccessible());

  HyperTextAccessible* editable =
      (State() & states::EDITABLE) != 0 ? AsHyperText() : nullptr;

  int32_t start = aStartOffset, end = aEndOffset;
  // If the accessible is an editable, set the virtual cursor position
  // to its caret offset. Otherwise use the document's virtual cursor
  // position as a starting offset.
  if (editable) {
    start = end = editable->CaretOffset();
  }

  uint16_t pivotGranularity = nsIAccessiblePivot::LINE_BOUNDARY;
  switch (aGranularity) {
    case 1:  // MOVEMENT_GRANULARITY_CHARACTER
      pivotGranularity = nsIAccessiblePivot::CHAR_BOUNDARY;
      break;
    case 2:  // MOVEMENT_GRANULARITY_WORD
      pivotGranularity = nsIAccessiblePivot::WORD_BOUNDARY;
      break;
    default:
      break;
  }

  int32_t newOffset;
  Accessible* newAnchorBase = nullptr;
  if (aForward) {
    newAnchorBase = pivot.NextText(this, &start, &end, pivotGranularity);
    newOffset = end;
  } else {
    newAnchorBase = pivot.PrevText(this, &start, &end, pivotGranularity);
    newOffset = start;
  }
  LocalAccessible* newAnchor =
      newAnchorBase ? newAnchorBase->AsLocal() : nullptr;

  if (newAnchor && (start != aStartOffset || end != aEndOffset)) {
    if (IsTextLeaf() && newAnchor == LocalParent()) {
      // For paragraphs, divs, spans, etc., we put a11y focus on the text leaf
      // node instead of the HyperTextAccessible. However, Pivot will always
      // return a HyperTextAccessible. Android doesn't support text navigation
      // landing on an accessible which is different to the originating
      // accessible. Therefore, if we're still within the same text leaf,
      // translate the offsets to the text leaf.
      int32_t thisChild = IndexInParent();
      HyperTextAccessible* newHyper = newAnchor->AsHyperText();
      MOZ_ASSERT(newHyper);
      int32_t startChild = newHyper->GetChildIndexAtOffset(start);
      // We use end - 1 because the end offset is exclusive, so end itself
      // might be associated with the next child.
      int32_t endChild = newHyper->GetChildIndexAtOffset(end - 1);
      if (startChild == thisChild && endChild == thisChild) {
        // We've landed within the same text leaf.
        newAnchor = this;
        int32_t thisOffset = newHyper->GetChildOffset(thisChild);
        start -= thisOffset;
        end -= thisOffset;
      }
    }
    RefPtr<AccEvent> event = new AccVCChangeEvent(
        newAnchor->Document(), this, aStartOffset, aEndOffset, newAnchor, start,
        end, nsIAccessiblePivot::REASON_NONE, pivotGranularity, eFromUserInput);
    nsEventShell::FireEvent(event);
  }

  // If we are in an editable, move the caret to the new virtual cursor
  // offset.
  if (editable) {
    if (aSelect) {
      int32_t anchor = editable->CaretOffset();
      if (editable->SelectionCount()) {
        int32_t startSel, endSel;
        GetSelectionOrCaret(&startSel, &endSel);
        anchor = startSel == anchor ? endSel : startSel;
      }
      editable->SetSelectionBoundsAt(0, anchor, newOffset);
    } else {
      editable->SetCaretOffset(newOffset);
    }
  }
}

void AccessibleWrap::SetSelection(int32_t aStart, int32_t aEnd) {
  if (HyperTextAccessible* textAcc = AsHyperText()) {
    if (aStart == aEnd) {
      textAcc->SetCaretOffset(aStart);
    } else {
      textAcc->SetSelectionBoundsAt(0, aStart, aEnd);
    }
  }
}

void AccessibleWrap::Cut() {
  if ((State() & states::EDITABLE) == 0) {
    return;
  }

  if (HyperTextAccessible* textAcc = AsHyperText()) {
    int32_t startSel, endSel;
    GetSelectionOrCaret(&startSel, &endSel);
    textAcc->CutText(startSel, endSel);
  }
}

void AccessibleWrap::Copy() {
  if (HyperTextAccessible* textAcc = AsHyperText()) {
    int32_t startSel, endSel;
    GetSelectionOrCaret(&startSel, &endSel);
    textAcc->CopyText(startSel, endSel);
  }
}

void AccessibleWrap::Paste() {
  if ((State() & states::EDITABLE) == 0) {
    return;
  }

  if (IsHyperText()) {
    RefPtr<HyperTextAccessible> textAcc = AsHyperText();
    int32_t startSel, endSel;
    GetSelectionOrCaret(&startSel, &endSel);
    if (startSel != endSel) {
      textAcc->DeleteText(startSel, endSel);
    }
    textAcc->PasteText(startSel);
  }
}

void AccessibleWrap::GetSelectionOrCaret(int32_t* aStartOffset,
                                         int32_t* aEndOffset) {
  *aStartOffset = *aEndOffset = -1;
  if (HyperTextAccessible* textAcc = AsHyperText()) {
    if (!textAcc->SelectionBoundsAt(0, aStartOffset, aEndOffset)) {
      *aStartOffset = *aEndOffset = textAcc->CaretOffset();
    }
  }
}

uint32_t AccessibleWrap::GetFlags(role aRole, uint64_t aState,
                                  uint8_t aActionCount) {
  uint32_t flags = 0;
  if (aState & states::CHECKABLE) {
    flags |= java::SessionAccessibility::FLAG_CHECKABLE;
  }

  if (aState & states::CHECKED) {
    flags |= java::SessionAccessibility::FLAG_CHECKED;
  }

  if (aState & states::INVALID) {
    flags |= java::SessionAccessibility::FLAG_CONTENT_INVALID;
  }

  if (aState & states::EDITABLE) {
    flags |= java::SessionAccessibility::FLAG_EDITABLE;
  }

  if (aActionCount && aRole != roles::TEXT_LEAF) {
    flags |= java::SessionAccessibility::FLAG_CLICKABLE;
  }

  if (aState & states::ENABLED) {
    flags |= java::SessionAccessibility::FLAG_ENABLED;
  }

  if (aState & states::FOCUSABLE) {
    flags |= java::SessionAccessibility::FLAG_FOCUSABLE;
  }

  if (aState & states::FOCUSED) {
    flags |= java::SessionAccessibility::FLAG_FOCUSED;
  }

  if (aState & states::MULTI_LINE) {
    flags |= java::SessionAccessibility::FLAG_MULTI_LINE;
  }

  if (aState & states::SELECTABLE) {
    flags |= java::SessionAccessibility::FLAG_SELECTABLE;
  }

  if (aState & states::SELECTED) {
    flags |= java::SessionAccessibility::FLAG_SELECTED;
  }

  if (aState & states::EXPANDABLE) {
    flags |= java::SessionAccessibility::FLAG_EXPANDABLE;
  }

  if (aState & states::EXPANDED) {
    flags |= java::SessionAccessibility::FLAG_EXPANDED;
  }

  if ((aState & (states::INVISIBLE | states::OFFSCREEN)) == 0) {
    flags |= java::SessionAccessibility::FLAG_VISIBLE_TO_USER;
  }

  if (aRole == roles::PASSWORD_TEXT) {
    flags |= java::SessionAccessibility::FLAG_PASSWORD;
  }

  return flags;
}

void AccessibleWrap::GetRoleDescription(role aRole, AccAttributes* aAttributes,
                                        nsAString& aGeckoRole,
                                        nsAString& aRoleDescription) {
  if (aRole == roles::HEADING && aAttributes) {
    // The heading level is an attribute, so we need that.
    nsAutoString headingLevel;
    if (aAttributes->GetAttribute(nsGkAtoms::level, headingLevel)) {
      nsAutoString token(u"heading-");
      token.Append(headingLevel);
      if (LocalizeString(token, aRoleDescription)) {
        return;
      }
    }
  }

  if ((aRole == roles::LANDMARK || aRole == roles::REGION) && aAttributes) {
    nsAutoString xmlRoles;
    if (aAttributes->GetAttribute(nsGkAtoms::xmlroles, xmlRoles)) {
      nsWhitespaceTokenizer tokenizer(xmlRoles);
      while (tokenizer.hasMoreTokens()) {
        if (LocalizeString(tokenizer.nextToken(), aRoleDescription)) {
          return;
        }
      }
    }
  }

  GetAccService()->GetStringRole(aRole, aGeckoRole);
  LocalizeString(aGeckoRole, aRoleDescription);
}

int32_t AccessibleWrap::AndroidClass(Accessible* aAccessible) {
  return GetVirtualViewID(aAccessible) == SessionAccessibility::kNoID
             ? java::SessionAccessibility::CLASSNAME_WEBVIEW
             : GetAndroidClass(aAccessible->Role());
}

int32_t AccessibleWrap::GetVirtualViewID(Accessible* aAccessible) {
  if (aAccessible->IsLocal()) {
    return static_cast<AccessibleWrap*>(aAccessible)->mID;
  }

  return static_cast<int32_t>(aAccessible->AsRemote()->GetWrapper());
}

void AccessibleWrap::SetVirtualViewID(Accessible* aAccessible,
                                      int32_t aVirtualViewID) {
  if (aAccessible->IsLocal()) {
    static_cast<AccessibleWrap*>(aAccessible)->mID = aVirtualViewID;
  } else {
    aAccessible->AsRemote()->SetWrapper(static_cast<uintptr_t>(aVirtualViewID));
  }
}

int32_t AccessibleWrap::GetAndroidClass(role aRole) {
#define ROLE(geckoRole, stringRole, atkRole, macRole, macSubrole, msaaRole, \
             ia2Role, androidClass, nameRule)                               \
  case roles::geckoRole:                                                    \
    return androidClass;

  switch (aRole) {
#include "RoleMap.h"
    default:
      return java::SessionAccessibility::CLASSNAME_VIEW;
  }

#undef ROLE
}

int32_t AccessibleWrap::GetInputType(const nsString& aInputTypeAttr) {
  if (aInputTypeAttr.EqualsIgnoreCase("email")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT |
           java::sdk::InputType::TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("number")) {
    return java::sdk::InputType::TYPE_CLASS_NUMBER;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("password")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT |
           java::sdk::InputType::TYPE_TEXT_VARIATION_WEB_PASSWORD;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("tel")) {
    return java::sdk::InputType::TYPE_CLASS_PHONE;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("text")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT |
           java::sdk::InputType::TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("url")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT |
           java::sdk::InputType::TYPE_TEXT_VARIATION_URI;
  }

  return 0;
}

void AccessibleWrap::GetTextEquiv(nsString& aText) {
  if (nsTextEquivUtils::HasNameRule(this, eNameFromSubtreeIfReqRule)) {
    // This is an accessible that normally doesn't get its name from its
    // subtree, so we collect the text equivalent explicitly.
    nsTextEquivUtils::GetTextEquivFromSubtree(this, aText);
  } else {
    Name(aText);
  }
}

bool AccessibleWrap::HandleLiveRegionEvent(AccEvent* aEvent) {
  auto eventType = aEvent->GetEventType();
  if (eventType != nsIAccessibleEvent::EVENT_TEXT_INSERTED &&
      eventType != nsIAccessibleEvent::EVENT_NAME_CHANGE) {
    // XXX: Right now only announce text inserted events. aria-relevant=removals
    // is potentially on the chopping block[1]. We also don't support editable
    // text because we currently can't descern the source of the change[2].
    // 1. https://github.com/w3c/aria/issues/712
    // 2. https://bugzilla.mozilla.org/show_bug.cgi?id=1531189
    return false;
  }

  if (aEvent->IsFromUserInput()) {
    return false;
  }

  RefPtr<AccAttributes> attributes = new AccAttributes();
  nsAccUtils::SetLiveContainerAttributes(attributes, this);
  nsString live;
  if (!attributes->GetAttribute(nsGkAtoms::containerLive, live)) {
    return false;
  }

  uint16_t priority = live.EqualsIgnoreCase("assertive")
                          ? nsIAccessibleAnnouncementEvent::ASSERTIVE
                          : nsIAccessibleAnnouncementEvent::POLITE;

  Maybe<bool> atomic =
      attributes->GetAttribute<bool>(nsGkAtoms::containerAtomic);
  LocalAccessible* announcementTarget = this;
  nsAutoString announcement;
  if (atomic && *atomic) {
    LocalAccessible* atomicAncestor = nullptr;
    for (LocalAccessible* parent = announcementTarget; parent;
         parent = parent->LocalParent()) {
      dom::Element* element = parent->Elm();
      if (element &&
          element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_atomic,
                               nsGkAtoms::_true, eCaseMatters)) {
        atomicAncestor = parent;
        break;
      }
    }

    if (atomicAncestor) {
      announcementTarget = atomicAncestor;
      static_cast<AccessibleWrap*>(atomicAncestor)->GetTextEquiv(announcement);
    }
  } else {
    GetTextEquiv(announcement);
  }

  announcement.CompressWhitespace();
  if (announcement.IsEmpty()) {
    return false;
  }

  announcementTarget->Announce(announcement, priority);
  return true;
}
