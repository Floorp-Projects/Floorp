/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULElementAccessibles.h"

#include "LocalAccessible-inl.h"
#include "BaseAccessibles.h"
#include "DocAccessible-inl.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "mozilla/a11y/Role.h"
#include "States.h"
#include "TextUpdater.h"

#ifdef A11Y_LOG
#  include "Logging.h"
#endif

#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsXULElement.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULLabelAccessible
////////////////////////////////////////////////////////////////////////////////

XULLabelAccessible::XULLabelAccessible(nsIContent* aContent,
                                       DocAccessible* aDoc)
    : HyperTextAccessible(aContent, aDoc) {
  mType = eXULLabelType;
}

void XULLabelAccessible::Shutdown() {
  mValueTextLeaf = nullptr;
  HyperTextAccessible::Shutdown();
}

void XULLabelAccessible::DispatchClickEvent(nsIContent* aContent,
                                            uint32_t aActionIndex) const {
  // Bug 1578140: For labels inside buttons, The base implementation of
  // DispatchClickEvent doesn't fire a command event on the button.
  RefPtr<nsXULElement> el = nsXULElement::FromNodeOrNull(aContent);
  if (el) {
    el->Click(mozilla::dom::CallerType::System);
  }
}

ENameValueFlag XULLabelAccessible::NativeName(nsString& aName) const {
  // if the value attr doesn't exist, the screen reader must get the accessible
  // text from the accessible text interface or from the children
  if (mValueTextLeaf) return mValueTextLeaf->Name(aName);

  return LocalAccessible::NativeName(aName);
}

role XULLabelAccessible::NativeRole() const { return roles::LABEL; }

uint64_t XULLabelAccessible::NativeState() const {
  // Labels and description have read only state
  // They are not focusable or selectable
  return HyperTextAccessible::NativeState() | states::READONLY;
}

Relation XULLabelAccessible::RelationByType(RelationType aType) const {
  Relation rel = HyperTextAccessible::RelationByType(aType);

  // The label for xul:groupbox is generated from the first xul:label
  if (aType == RelationType::LABEL_FOR) {
    LocalAccessible* parent = LocalParent();
    if (parent && parent->Role() == roles::GROUPING &&
        parent->LocalChildAt(0) == this) {
      nsIContent* parentContent = parent->GetContent();
      if (parentContent && parentContent->IsXULElement(nsGkAtoms::groupbox)) {
        rel.AppendTarget(parent);
      }
    }
  }

  return rel;
}

void XULLabelAccessible::UpdateLabelValue(const nsString& aValue) {
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eText)) {
    logging::MsgBegin("TEXT", "text may be changed (xul:label @value update)");
    logging::Node("container", mContent);
    logging::MsgEntry("old text '%s'",
                      NS_ConvertUTF16toUTF8(mValueTextLeaf->Text()).get());
    logging::MsgEntry("new text: '%s'", NS_ConvertUTF16toUTF8(aValue).get());
    logging::MsgEnd();
  }
#endif

  TextUpdater::Run(mDoc, mValueTextLeaf, aValue);
}

////////////////////////////////////////////////////////////////////////////////
// XULLabelTextLeafAccessible
////////////////////////////////////////////////////////////////////////////////

role XULLabelTextLeafAccessible::NativeRole() const { return roles::TEXT_LEAF; }

uint64_t XULLabelTextLeafAccessible::NativeState() const {
  return TextLeafAccessible::NativeState() | states::READONLY;
}

////////////////////////////////////////////////////////////////////////////////
// XULTooltipAccessible
////////////////////////////////////////////////////////////////////////////////

XULTooltipAccessible::XULTooltipAccessible(nsIContent* aContent,
                                           DocAccessible* aDoc)
    : LeafAccessible(aContent, aDoc) {
  mType = eXULTooltipType;
}

uint64_t XULTooltipAccessible::NativeState() const {
  return LeafAccessible::NativeState() | states::READONLY;
}

role XULTooltipAccessible::NativeRole() const { return roles::TOOLTIP; }

////////////////////////////////////////////////////////////////////////////////
// XULLinkAccessible
////////////////////////////////////////////////////////////////////////////////

XULLinkAccessible::XULLinkAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : XULLabelAccessible(aContent, aDoc) {}

XULLinkAccessible::~XULLinkAccessible() {}

////////////////////////////////////////////////////////////////////////////////
// XULLinkAccessible: LocalAccessible

void XULLinkAccessible::Value(nsString& aValue) const {
  aValue.Truncate();

  mContent->AsElement()->GetAttr(nsGkAtoms::href, aValue);
}

ENameValueFlag XULLinkAccessible::NativeName(nsString& aName) const {
  mContent->AsElement()->GetAttr(nsGkAtoms::value, aName);
  if (!aName.IsEmpty()) return eNameOK;

  nsTextEquivUtils::GetNameFromSubtree(this, aName);
  return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
}

role XULLinkAccessible::NativeRole() const { return roles::LINK; }

uint64_t XULLinkAccessible::NativeLinkState() const { return states::LINKED; }

bool XULLinkAccessible::HasPrimaryAction() const { return true; }

void XULLinkAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();

  if (aIndex == eAction_Jump) aName.AssignLiteral("jump");
}

////////////////////////////////////////////////////////////////////////////////
// XULLinkAccessible: HyperLinkAccessible

bool XULLinkAccessible::IsLink() const {
  // Expose HyperLinkAccessible unconditionally.
  return true;
}

uint32_t XULLinkAccessible::StartOffset() {
  // If XUL link accessible is not contained by hypertext accessible then
  // start offset matches index in parent because the parent doesn't contains
  // a text.
  // XXX: accessible parent of XUL link accessible should be a hypertext
  // accessible.
  if (LocalAccessible::IsLink()) return LocalAccessible::StartOffset();
  return IndexInParent();
}

uint32_t XULLinkAccessible::EndOffset() {
  if (LocalAccessible::IsLink()) return LocalAccessible::EndOffset();
  return IndexInParent() + 1;
}

already_AddRefed<nsIURI> XULLinkAccessible::AnchorURIAt(
    uint32_t aAnchorIndex) const {
  if (aAnchorIndex != 0) return nullptr;

  nsAutoString href;
  mContent->AsElement()->GetAttr(nsGkAtoms::href, href);

  dom::Document* document = mContent->OwnerDoc();

  nsCOMPtr<nsIURI> anchorURI;
  NS_NewURI(getter_AddRefs(anchorURI), href,
            document->GetDocumentCharacterSet(), mContent->GetBaseURI());

  return anchorURI.forget();
}
