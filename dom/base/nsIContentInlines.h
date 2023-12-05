/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIContentInlines_h
#define nsIContentInlines_h

#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsAtom.h"
#include "nsIFrame.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"

inline bool nsIContent::IsInHTMLDocument() const {
  return OwnerDoc()->IsHTMLDocument();
}

inline bool nsIContent::IsInChromeDocument() const {
  return nsContentUtils::IsChromeDoc(OwnerDoc());
}

inline void nsIContent::SetPrimaryFrame(nsIFrame* aFrame) {
  MOZ_ASSERT(IsInUncomposedDoc() || IsInShadowTree(), "This will end badly!");

  // <area> is known to trigger this, see bug 749326 and bug 135040.
  MOZ_ASSERT(IsHTMLElement(nsGkAtoms::area) || !aFrame || !mPrimaryFrame ||
                 aFrame == mPrimaryFrame,
             "Losing track of existing primary frame");

  if (aFrame) {
    MOZ_ASSERT(!aFrame->IsPlaceholderFrame());
    if (MOZ_LIKELY(!IsHTMLElement(nsGkAtoms::area)) ||
        aFrame->GetContent() == this) {
      aFrame->SetIsPrimaryFrame(true);
    }
  } else if (nsIFrame* currentPrimaryFrame = GetPrimaryFrame()) {
    if (MOZ_LIKELY(!IsHTMLElement(nsGkAtoms::area)) ||
        currentPrimaryFrame->GetContent() == this) {
      currentPrimaryFrame->SetIsPrimaryFrame(false);
    }
  }

  mPrimaryFrame = aFrame;
}

inline mozilla::dom::ShadowRoot* nsIContent::GetShadowRoot() const {
  if (!IsElement()) {
    return nullptr;
  }

  return AsElement()->GetShadowRoot();
}

template <nsINode::FlattenedParentType aType>
static inline nsINode* GetFlattenedTreeParentNode(const nsINode* aNode) {
  if (!aNode->IsContent()) {
    return nullptr;
  }

  nsINode* parent = aNode->GetParentNode();
  if (!parent || !parent->IsContent()) {
    return parent;
  }

  const nsIContent* content = aNode->AsContent();
  nsIContent* parentAsContent = parent->AsContent();

  if (aType == nsINode::eForStyle &&
      content->IsRootOfNativeAnonymousSubtree() &&
      parentAsContent == content->OwnerDoc()->GetRootElement()) {
    const bool docLevel =
        content->GetProperty(nsGkAtoms::docLevelNativeAnonymousContent);
    return docLevel ? content->OwnerDocAsNode() : parent;
  }

  if (content->IsRootOfNativeAnonymousSubtree()) {
    return parent;
  }

  if (parentAsContent->GetShadowRoot()) {
    // If it's not assigned to any slot it's not part of the flat tree, and thus
    // we return null.
    return content->GetAssignedSlot();
  }

  if (parentAsContent->IsInShadowTree()) {
    if (auto* slot = mozilla::dom::HTMLSlotElement::FromNode(parentAsContent)) {
      // If the assigned nodes list is empty, we're fallback content which is
      // active, otherwise we are not part of the flat tree.
      return slot->AssignedNodes().IsEmpty() ? parent : nullptr;
    }

    if (auto* shadowRoot =
            mozilla::dom::ShadowRoot::FromNode(parentAsContent)) {
      return shadowRoot->GetHost();
    }
  }

  // Common case.
  return parent;
}

inline nsINode* nsINode::GetFlattenedTreeParentNode() const {
  return ::GetFlattenedTreeParentNode<nsINode::eNotForStyle>(this);
}

inline nsIContent* nsIContent::GetFlattenedTreeParent() const {
  nsINode* parent = GetFlattenedTreeParentNode();
  return (parent && parent->IsContent()) ? parent->AsContent() : nullptr;
}

inline bool nsIContent::IsEventAttributeName(nsAtom* aName) {
  const char16_t* name = aName->GetUTF16String();
  if (name[0] != 'o' || name[1] != 'n') {
    return false;
  }

  return IsEventAttributeNameInternal(aName);
}

inline nsINode* nsINode::GetFlattenedTreeParentNodeForStyle() const {
  return ::GetFlattenedTreeParentNode<nsINode::eForStyle>(this);
}

inline bool nsINode::NodeOrAncestorHasDirAuto() const {
  return AncestorHasDirAuto() || (IsElement() && AsElement()->HasDirAuto());
}

inline bool nsINode::IsEditable() const {
  if (HasFlag(NODE_IS_EDITABLE)) {
    // The node is in an editable contentEditable subtree.
    return true;
  }

  // All editable anonymous content should be made explicitly editable via the
  // NODE_IS_EDITABLE flag.
  if (IsInNativeAnonymousSubtree()) {
    return false;
  }

  // Check if the node is in a document and the document is in designMode.
  return IsInDesignMode();
}

inline bool nsINode::IsEditingHost() const {
  if (!IsInComposedDoc() || IsInDesignMode() || !IsEditable() ||
      IsInNativeAnonymousSubtree()) {
    return false;
  }
  nsIContent* const parent = GetParent();
  return !parent ||  // The root element (IsInComposedDoc() is checked above)
         !parent->IsEditable();  // or an editable node in a non-editable one
}

inline bool nsINode::IsInDesignMode() const {
  if (!OwnerDoc()->HasFlag(NODE_IS_EDITABLE)) {
    return false;
  }

  if (IsDocument()) {
    return HasFlag(NODE_IS_EDITABLE);
  }

  // NOTE(emilio): If you change this to be the composed doc you also need to
  // change NotifyEditableStateChange() in Document.cpp.
  // NOTE(masayuki): Perhaps, we should keep this behavior because of
  // web-compat.
  if (IsInUncomposedDoc() && GetUncomposedDoc()->HasFlag(NODE_IS_EDITABLE)) {
    return true;
  }

  // FYI: In design mode, form controls don't work as usual.  For example,
  //      <input type=text> isn't focusable but can be deleted and replaced
  //      with typed text. <select> is also not focusable but always selected
  //      all to be deleted or replaced.  On the other hand, newer controls
  //      don't behave as the traditional controls.  For example, data/time
  //      picker can be opened and change the value from the picker.  And also
  //      the buttons of <video controls> work as usual.  On the other hand,
  //      their UI (i.e., nodes in their shadow tree) are not editable.
  //      Therefore, we need special handling for nodes in anonymous subtree
  //      unless we fix <https://bugzilla.mozilla.org/show_bug.cgi?id=1734512>.

  // If the shadow host is not in design mode, this can never be in design
  // mode.  Otherwise, the content is never editable by design mode of
  // composed document. If we're in a native anonymous subtree, we should
  // consider it with the host.
  if (IsInNativeAnonymousSubtree()) {
    nsIContent* host = GetClosestNativeAnonymousSubtreeRootParentOrHost();
    MOZ_DIAGNOSTIC_ASSERT(host != this);
    return host && host->IsInDesignMode();
  }

  // Otherwise, i.e., when it's in a shadow tree which is not created by us,
  // the node is not editable by design mode (but it's possible that it may be
  // editable if this node is in `contenteditable` element in the shadow tree).
  return false;
}

inline void nsIContent::HandleInsertionToOrRemovalFromSlot() {
  using mozilla::dom::HTMLSlotElement;

  MOZ_ASSERT(GetParentElement());
  if (!IsInShadowTree() || IsRootOfNativeAnonymousSubtree()) {
    return;
  }
  HTMLSlotElement* slot = HTMLSlotElement::FromNode(mParent);
  if (!slot) {
    return;
  }
  // If parent's root is a shadow root, and parent is a slot whose
  // assigned nodes is the empty list, then run signal a slot change for
  // parent.
  if (slot->AssignedNodes().IsEmpty()) {
    slot->EnqueueSlotChangeEvent();
  }
}

inline void nsIContent::HandleShadowDOMRelatedInsertionSteps(bool aHadParent) {
  using mozilla::dom::Element;
  using mozilla::dom::ShadowRoot;

  if (!aHadParent) {
    if (Element* parentElement = Element::FromNode(mParent)) {
      if (ShadowRoot* shadow = parentElement->GetShadowRoot()) {
        shadow->MaybeSlotHostChild(*this);
      }
      HandleInsertionToOrRemovalFromSlot();
    }
  }
}

inline void nsIContent::HandleShadowDOMRelatedRemovalSteps(bool aNullParent) {
  using mozilla::dom::Element;
  using mozilla::dom::ShadowRoot;

  if (aNullParent) {
    // FIXME(emilio, bug 1577141): FromNodeOrNull rather than just FromNode
    // because XBL likes to call UnbindFromTree at very odd times (with already
    // disconnected anonymous content subtrees).
    if (Element* parentElement = Element::FromNodeOrNull(mParent)) {
      if (ShadowRoot* shadow = parentElement->GetShadowRoot()) {
        shadow->MaybeUnslotHostChild(*this);
      }
      HandleInsertionToOrRemovalFromSlot();
    }
  }
}

#endif  // nsIContentInlines_h
