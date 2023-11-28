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
#include "nsIAccessiblePivot.h"
#include "nsAccUtils.h"
#include "nsTextEquivUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "RootAccessible.h"
#include "TextLeafRange.h"

#include "mozilla/a11y/PDocAccessibleChild.h"
#include "mozilla/jni/GeckoBundleUtils.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/Maybe.h"

// icu TRUE conflicting with java::sdk::Boolean::TRUE()
// https://searchfox.org/mozilla-central/rev/ce02064d8afc8673cef83c92896ee873bd35e7ae/intl/icu/source/common/unicode/umachine.h#265
// https://searchfox.org/mozilla-central/source/__GENERATED__/widget/android/bindings/JavaBuiltins.h#78
#ifdef TRUE
#  undef TRUE
#endif

using namespace mozilla::a11y;
using mozilla::Maybe;

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

  nsresult rv = LocalAccessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  accessible->HandleLiveRegionEvent(aEvent);

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
  Accessible* pivotRoot = nullptr;
  if (aAccessible->IsRemote()) {
    // If this is a remote accessible provide the top level
    // remote doc as the pivot root for thread safety reasons.
    DocAccessibleParent* doc = aAccessible->AsRemote()->Document();
    while (doc && !doc->IsTopLevel()) {
      doc = doc->ParentDoc();
    }
    MOZ_ASSERT(doc, "Failed to get top level DocAccessibleParent");
    pivotRoot = doc;
  }
  a11y::Pivot pivot(pivotRoot);
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

Accessible* AccessibleWrap::ExploreByTouch(Accessible* aAccessible, float aX,
                                           float aY) {
  Accessible* root;
  if (LocalAccessible* local = aAccessible->AsLocal()) {
    root = local->RootAccessible();
  } else {
    // If this is a RemoteAccessible, provide the top level
    // remote doc as the pivot root for thread safety reasons.
    DocAccessibleParent* doc = aAccessible->AsRemote()->Document();
    while (doc && !doc->IsTopLevel()) {
      doc = doc->ParentDoc();
    }
    MOZ_ASSERT(doc, "Failed to get top level DocAccessibleParent");
    root = doc;
  }
  a11y::Pivot pivot(root);
  TraversalRule rule(java::SessionAccessibility::HTML_GRANULARITY_DEFAULT,
                     aAccessible->IsLocal());
  Accessible* result = pivot.AtPoint(aX, aY, rule);
  if (result == aAccessible) {
    return nullptr;
  }
  return result;
}

static TextLeafPoint ToTextLeafPoint(Accessible* aAccessible, int32_t aOffset) {
  if (HyperTextAccessibleBase* ht = aAccessible->AsHyperTextBase()) {
    return ht->ToTextLeafPoint(aOffset);
  }

  return TextLeafPoint(aAccessible, aOffset);
}

Maybe<std::pair<int32_t, int32_t>> AccessibleWrap::NavigateText(
    Accessible* aAccessible, int32_t aGranularity, int32_t aStartOffset,
    int32_t aEndOffset, bool aForward, bool aSelect) {
  int32_t startOffset = aStartOffset;
  int32_t endOffset = aEndOffset;
  if (startOffset == -1) {
    MOZ_ASSERT(endOffset == -1,
               "When start offset is unset, end offset should be too");
    startOffset = aForward ? 0 : nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT;
    endOffset = aForward ? 0 : nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT;
  }

  // If the accessible is an editable, set the virtual cursor position
  // to its caret offset. Otherwise use the document's virtual cursor
  // position as a starting offset.
  if (aAccessible->State() & states::EDITABLE) {
    startOffset = endOffset = aAccessible->AsHyperTextBase()->CaretOffset();
  }

  TextLeafRange currentRange =
      TextLeafRange(ToTextLeafPoint(aAccessible, startOffset),
                    ToTextLeafPoint(aAccessible, endOffset));
  uint16_t startBoundaryType = nsIAccessibleText::BOUNDARY_LINE_START;
  uint16_t endBoundaryType = nsIAccessibleText::BOUNDARY_LINE_END;
  switch (aGranularity) {
    case 1:  // MOVEMENT_GRANULARITY_CHARACTER
      startBoundaryType = nsIAccessibleText::BOUNDARY_CHAR;
      endBoundaryType = nsIAccessibleText::BOUNDARY_CHAR;
      break;
    case 2:  // MOVEMENT_GRANULARITY_WORD
      startBoundaryType = nsIAccessibleText::BOUNDARY_WORD_START;
      endBoundaryType = nsIAccessibleText::BOUNDARY_WORD_END;
      break;
    default:
      break;
  }

  TextLeafRange resultRange;

  if (aForward) {
    resultRange.SetEnd(
        currentRange.End().FindBoundary(endBoundaryType, eDirNext));
    resultRange.SetStart(
        resultRange.End().FindBoundary(startBoundaryType, eDirPrevious));
  } else {
    resultRange.SetStart(
        currentRange.Start().FindBoundary(startBoundaryType, eDirPrevious));
    resultRange.SetEnd(
        resultRange.Start().FindBoundary(endBoundaryType, eDirNext));
  }

  if (!resultRange.Crop(aAccessible)) {
    // If the new range does not intersect at all with the given
    // accessible/container this navigation has failed or reached an edge.
    return Nothing();
  }

  if (resultRange == currentRange || resultRange.Start() == resultRange.End()) {
    // If the result range equals the current range, or if the result range is
    // collapsed, we failed or reached an edge.
    return Nothing();
  }

  if (HyperTextAccessibleBase* ht = aAccessible->AsHyperTextBase()) {
    DebugOnly<bool> ok = false;
    std::tie(ok, startOffset) = ht->TransformOffset(
        resultRange.Start().mAcc, resultRange.Start().mOffset, false);
    MOZ_ASSERT(ok, "Accessible of range start should be in container.");

    std::tie(ok, endOffset) = ht->TransformOffset(
        resultRange.End().mAcc, resultRange.End().mOffset, false);
    MOZ_ASSERT(ok, "Accessible range end should be in container.");
  } else {
    startOffset = resultRange.Start().mOffset;
    endOffset = resultRange.End().mOffset;
  }

  return Some(std::make_pair(startOffset, endOffset));
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
#define ROLE(geckoRole, stringRole, ariaRole, atkRole, macRole, macSubrole, \
             msaaRole, ia2Role, androidClass, nameRule)                     \
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
          nsAccUtils::ARIAAttrValueIs(element, nsGkAtoms::aria_atomic,
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
