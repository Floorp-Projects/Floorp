/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  Static case
  ===========
  When we see a new content node with @dir=auto from the parser, we set the
  NodeHasDirAuto flag on the node.  We won't have enough information to
  decide the directionality of the node at this point.

  When we bind a new content node to the document, if its parent has either of
  the NodeAncestorHasDirAuto or NodeHasDirAuto flags, we set the
  NodeAncestorHasDirAuto flag on the node.

  When a new input with @type=text/search/tel/url/email and @dir=auto is added
  from the parser, we resolve the directionality based on its @value.

  When a new text node with non-neutral content is appended to a textarea
  element with NodeHasDirAuto, if the directionality of the textarea element
  is still unresolved, it is resolved based on the value of the text node.
  Elements with unresolved directionality behave as Ltr.

  When a new text node with non-neutral content is appended to an element that
  is not a textarea but has either of the NodeAncestorHasDirAuto or
  NodeHasDirAuto flags, we walk up the parent chain while the
  NodeAncestorHasDirAuto flag is present, and when we reach an element with
  NodeHasDirAuto and no resolved directionality, we resolve the directionality
  based on the contents of the text node and cease walking the parent chain.
  Note that we should ignore elements with NodeHasDirAuto with resolved
  directionality, so that the second text node in this example tree doesn't
  affect the directionality of the div:

  <div dir=auto>
    <span>foo</span>
    <span>بار</span>
  </div>

  The parent chain walk will be aborted if we hit a script or style element, or
  if we hit an element with @dir=ltr or @dir=rtl.

  I will call this algorithm "upward propagation".

  Each text node keeps a flag if it might determine the directionality of any
  ancestor. This is useful to make dynamic changes more efficient.

  Handling dynamic changes
  ========================

  We need to handle the following cases:

  1. When the value of an input element with @type=text/search/tel/url/email is
  changed, if it has NodeHasDirAuto, we update the resolved directionality.

  2. When the dir attribute is changed from something else (including the case
  where it doesn't exist) to auto on a textarea or an input element with
  @type=text/search/tel/url/email, we set the NodeHasDirAuto flag and resolve
  the directionality based on the value of the element.

  3. When the dir attribute is changed from something else (including the case
  where it doesn't exist) to auto on any element except case 1 above and the bdi
  element, we run the following algorithm:
  * We set the NodeHasDirAuto flag.
  * If the element doesn't have the NodeAncestorHasDirAuto flag, we set the
  NodeAncestorHasDirAuto flag on all of its child nodes.  (Note that if the
  element does have NodeAncestorHasDirAuto, all of its children should
  already have this flag too.  We can assert this in debug builds.)
  * To resolve the directionality of the element, we run the algorithm explained
  in
  http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#the-dir-attribute
  (I'll call this the "downward propagation algorithm".) by walking the child
  subtree in tree order.  Note that an element with @dir=auto should not affect
  other elements in its document with @dir=auto.  So there is no need to walk up
  the parent chain in this case.

  3a. When the dir attribute is set to any valid value on an element that didn't
  have a valid dir attribute before, this means that any descendant of that
  element will not affect the directionality of any of its ancestors. So we need
  to check whether any text node descendants of the element can set the dir of
  any ancestor, and whether the elements whose direction they set are ancestors
  of the element. If so, we need to rerun the downward propagation algorithm for
  those ancestors. That's done by OnSetDirAttr.

  4.  When the dir attribute is changed from auto to something else (including
  the case where it gets removed) on a textarea or an input element with
  @type=text/search/tel/url/email, we unset the NodeHasDirAuto flag and
  resolve the directionality based on the directionality of the value of the
  @dir attribute on element itself or its parent element.

  5. When the dir attribute is changed from auto to something else (including
  the case where it gets removed) on any element except case 4 above and the bdi
  element, we run the following algorithm:
  * We unset the NodeHasDirAuto flag.
  * If the element does not have the NodeAncestorHasDirAuto flag, we unset
  the NodeAncestorHasDirAuto flag on all of its child nodes, except those
  who are a descendant of another element with NodeHasDirAuto.  (Note that if
  the element has the NodeAncestorHasDirAuto flag, all of its child nodes
  should still retain the same flag.)
  * We resolve the directionality of the element based on the value of the @dir
  attribute on the element itself or its parent element.

  5a. When the dir attribute is removed or set to an invalid value on any
  element (except a bdi element) with the NodeAncestorHasDirAuto flag which
  previously had a valid dir attribute, it might have a text node descendant
  that did not previously affect the directionality of any of its ancestors but
  should now begin to affect them. We run OnSetDirAttr.

  6. When an element with @dir=auto is added to the document, we should handle
  it similar to the case 2/3 above.

  7. When an element with NodeHasDirAuto or NodeAncestorHasDirAuto is
  removed from the document, we should handle it similar to the case 4/5 above,
  except that we don't need to handle anything in the child subtree.

  8. When the contents of a text node is changed either from script or by the
  user, we need to run TextNode{WillChange,Changed}Direction, see inline docs
  for details.

  9. When a new text node is injected into a document, we need to run
  SetDirectionFromNewTextNode.

  10. When a text node is removed from a document, we need to run
  ResetDirectionSetByTextNode.

  11. If the value of the @dir attribute on a bdi element is changed to an
  invalid value (or if it's removed), determine the new directionality similar
  to the case 3 above.

  == Implemention Notes ==
  When a new node gets bound to the tree, the BindToTree function gets called.
  The reverse case is UnbindFromTree.
  When the contents of a text node change, CharacterData::SetTextInternal
  gets called.
  */

#include "mozilla/dom/DirectionalityUtils.h"

#include "nsINode.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/UnbindContext.h"
#include "mozilla/intl/UnicodeProperties.h"
#include "nsUnicodeProperties.h"
#include "nsTextFragment.h"
#include "nsAttrValue.h"
#include "nsTextNode.h"

namespace mozilla {

using mozilla::dom::Element;
using mozilla::dom::HTMLInputElement;
using mozilla::dom::HTMLSlotElement;
using mozilla::dom::ShadowRoot;

static nsIContent* GetParentOrHostOrSlot(const nsIContent* aContent) {
  if (HTMLSlotElement* slot = aContent->GetAssignedSlot()) {
    return slot;
  }
  if (nsIContent* parent = aContent->GetParent()) {
    return parent;
  }
  if (const ShadowRoot* sr = ShadowRoot::FromNode(aContent)) {
    return sr->GetHost();
  }
  return nullptr;
}

/**
 * Returns true if aElement is one of the elements whose text content should
 * affect its own direction, or the direction of ancestors with dir=auto.
 *
 * Note that this does not include <bdi>, whose content does affect its own
 * direction when it has dir=auto (which it has by default), so one needs to
 * test for it separately, e.g. with AffectsDirectionOfAncestors.
 * It *does* include textarea, because even if a textarea has dir=auto, it has
 * unicode-bidi: plaintext and is handled automatically in bidi resolution.
 * It also includes `input`, because it takes the `dir` value from its value
 * attribute, instead of the child nodes.
 */
static bool ParticipatesInAutoDirection(const nsIContent* aContent) {
  if (aContent->IsInNativeAnonymousSubtree()) {
    return false;
  }
  if (aContent->IsShadowRoot()) {
    return true;
  }
  dom::NodeInfo* ni = aContent->NodeInfo();
  return ni->NamespaceID() == kNameSpaceID_XHTML &&
         !ni->Equals(nsGkAtoms::script) && !ni->Equals(nsGkAtoms::style) &&
         !ni->Equals(nsGkAtoms::input) && !ni->Equals(nsGkAtoms::textarea);
}

/**
 * Returns true if aElement is one of the element whose text should affect the
 * direction of ancestors with dir=auto (though note that even if it returns
 * false it may affect its own direction, e.g. <bdi> or dir=auto itself)
 */
static bool AffectsDirectionOfAncestors(const Element* aElement) {
  return ParticipatesInAutoDirection(aElement) &&
         !aElement->IsHTMLElement(nsGkAtoms::bdi) && !aElement->HasFixedDir() &&
         !aElement->HasDirAuto();
}

/**
 * Returns the directionality of a Unicode character
 */
static Directionality GetDirectionFromChar(uint32_t ch) {
  switch (intl::UnicodeProperties::GetBidiClass(ch)) {
    case intl::BidiClass::RightToLeft:
    case intl::BidiClass::RightToLeftArabic:
      return Directionality::Rtl;

    case intl::BidiClass::LeftToRight:
      return Directionality::Ltr;

    default:
      return Directionality::Unset;
  }
}

inline static bool TextChildrenAffectDirAutoAncestor(nsIContent* aContent) {
  return ParticipatesInAutoDirection(aContent) &&
         aContent->NodeOrAncestorHasDirAuto();
}

inline static bool NodeAffectsDirAutoAncestor(nsTextNode* aTextNode) {
  nsIContent* parent = GetParentOrHostOrSlot(aTextNode);
  return parent && TextChildrenAffectDirAutoAncestor(parent) &&
         !aTextNode->IsInNativeAnonymousSubtree();
}

Directionality GetDirectionFromText(const char16_t* aText,
                                    const uint32_t aLength,
                                    uint32_t* aFirstStrong) {
  const char16_t* start = aText;
  const char16_t* end = aText + aLength;

  while (start < end) {
    uint32_t current = start - aText;
    uint32_t ch = *start++;

    if (start < end && NS_IS_SURROGATE_PAIR(ch, *start)) {
      ch = SURROGATE_TO_UCS4(ch, *start++);
      current++;
    }

    // Just ignore lone surrogates
    if (!IS_SURROGATE(ch)) {
      Directionality dir = GetDirectionFromChar(ch);
      if (dir != Directionality::Unset) {
        if (aFirstStrong) {
          *aFirstStrong = current;
        }
        return dir;
      }
    }
  }

  if (aFirstStrong) {
    *aFirstStrong = UINT32_MAX;
  }
  return Directionality::Unset;
}

static Directionality GetDirectionFromText(const char* aText,
                                           const uint32_t aLength,
                                           uint32_t* aFirstStrong = nullptr) {
  const char* start = aText;
  const char* end = aText + aLength;

  while (start < end) {
    uint32_t current = start - aText;
    unsigned char ch = (unsigned char)*start++;

    Directionality dir = GetDirectionFromChar(ch);
    if (dir != Directionality::Unset) {
      if (aFirstStrong) {
        *aFirstStrong = current;
      }
      return dir;
    }
  }

  if (aFirstStrong) {
    *aFirstStrong = UINT32_MAX;
  }
  return Directionality::Unset;
}

static Directionality GetDirectionFromText(const mozilla::dom::Text* aTextNode,
                                           uint32_t* aFirstStrong = nullptr) {
  const nsTextFragment* frag = &aTextNode->TextFragment();
  if (frag->Is2b()) {
    return GetDirectionFromText(frag->Get2b(), frag->GetLength(), aFirstStrong);
  }

  return GetDirectionFromText(frag->Get1b(), frag->GetLength(), aFirstStrong);
}

static nsTextNode* WalkDescendantsAndGetDirectionFromText(
    nsINode* aRoot, Directionality* aDirectionality) {
  nsIContent* child = aRoot->GetFirstChild();
  while (child) {
    if ((child->IsElement() &&
         !AffectsDirectionOfAncestors(child->AsElement())) ||
        child->GetAssignedSlot()) {
      child = child->GetNextNonChildNode(aRoot);
      continue;
    }

    if (auto* slot = HTMLSlotElement::FromNode(child)) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes = slot->AssignedNodes();
      for (uint32_t i = 0; i < assignedNodes.Length(); ++i) {
        nsIContent* assignedNode = assignedNodes[i]->AsContent();
        if (assignedNode->NodeType() == nsINode::TEXT_NODE) {
          auto* text = static_cast<nsTextNode*>(assignedNode);
          Directionality textNodeDir = GetDirectionFromText(text);
          if (textNodeDir != Directionality::Unset) {
            *aDirectionality = textNodeDir;
            return text;
          }
        } else if (assignedNode->IsElement() &&
                   AffectsDirectionOfAncestors(assignedNode->AsElement())) {
          nsTextNode* text = WalkDescendantsAndGetDirectionFromText(
              assignedNode, aDirectionality);
          if (text) {
            return text;
          }
        }
      }
    }

    if (child->NodeType() == nsINode::TEXT_NODE) {
      auto* text = static_cast<nsTextNode*>(child);
      Directionality textNodeDir = GetDirectionFromText(text);
      if (textNodeDir != Directionality::Unset) {
        *aDirectionality = textNodeDir;
        return text;
      }
    }
    child = child->GetNextNode(aRoot);
  }

  return nullptr;
}

/**
 * Set the directionality of a node with dir=auto as defined in
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#the-directionality
 *
 * @return the text node containing the character that determined the direction
 */
static nsTextNode* WalkDescendantsSetDirectionFromText(Element* aElement,
                                                       bool aNotify) {
  MOZ_ASSERT(aElement, "Must have an element");
  MOZ_ASSERT(aElement->HasDirAuto(), "Element must have dir=auto");

  if (!ParticipatesInAutoDirection(aElement)) {
    return nullptr;
  }

  Directionality textNodeDir = Directionality::Unset;

  // Check the text in Shadow DOM.
  if (ShadowRoot* shadowRoot = aElement->GetShadowRoot()) {
    nsTextNode* text =
        WalkDescendantsAndGetDirectionFromText(shadowRoot, &textNodeDir);
    if (text) {
      aElement->SetDirectionality(textNodeDir, aNotify);
      return text;
    }
  }

  // Check the text in light DOM.
  nsTextNode* text =
      WalkDescendantsAndGetDirectionFromText(aElement, &textNodeDir);
  if (text) {
    aElement->SetDirectionality(textNodeDir, aNotify);
    return text;
  }

  // We walked all the descendants without finding a text node with strong
  // directional characters. Set the directionality to Ltr
  aElement->SetDirectionality(Directionality::Ltr, aNotify);
  return nullptr;
}

Directionality GetParentDirectionality(const Element* aElement) {
  if (nsIContent* parent = GetParentOrHostOrSlot(aElement)) {
    if (ShadowRoot* shadow = ShadowRoot::FromNode(parent)) {
      parent = shadow->GetHost();
    }
    if (parent && parent->IsElement()) {
      // If the node doesn't have an explicit dir attribute with a valid value,
      // the directionality is the same as the parent element (but don't
      // propagate the parent directionality if it isn't set yet).
      Directionality parentDir = parent->AsElement()->GetDirectionality();
      if (parentDir != Directionality::Unset) {
        return parentDir;
      }
    }
  }
  return Directionality::Ltr;
}

Directionality RecomputeDirectionality(Element* aElement, bool aNotify) {
  MOZ_ASSERT(!aElement->HasDirAuto(),
             "RecomputeDirectionality called with dir=auto");

  if (aElement->HasValidDir()) {
    return aElement->GetDirectionality();
  }

  // https://html.spec.whatwg.org/multipage/dom.html#the-directionality:
  //
  // If the element is an input element whose type attribute is in the
  // Telephone state, and the dir attribute is not in a defined state
  // (i.e. it is not present or has an invalid value)
  //
  //     The directionality of the element is 'ltr'.
  if (auto* input = HTMLInputElement::FromNode(*aElement)) {
    if (input->ControlType() == FormControlType::InputTel) {
      aElement->SetDirectionality(Directionality::Ltr, aNotify);
      return Directionality::Ltr;
    }
  }

  const Directionality dir = GetParentDirectionality(aElement);
  aElement->SetDirectionality(dir, aNotify);
  return dir;
}

// Whether the element establishes its own directionality and the one of its
// descendants.
static inline bool IsBoundary(const Element& aElement) {
  return aElement.HasValidDir() || aElement.HasDirAuto();
}

static void SetDirectionalityOnDescendantsInternal(nsINode* aNode,
                                                   Directionality aDir,
                                                   bool aNotify) {
  if (auto* element = Element::FromNode(aNode)) {
    if (ShadowRoot* shadow = element->GetShadowRoot()) {
      SetDirectionalityOnDescendantsInternal(shadow, aDir, aNotify);
    }
  }

  for (nsIContent* child = aNode->GetFirstChild(); child;) {
    auto* element = Element::FromNode(child);
    if (!element) {
      child = child->GetNextNode(aNode);
      continue;
    }

    if (IsBoundary(*element) || element->GetAssignedSlot() ||
        element->GetDirectionality() == aDir) {
      // If the element is a directionality boundary, or it's assigned to a slot
      // (in which case it doesn't inherit the directionality from its direct
      // parent), or already has the right directionality, then we can skip the
      // whole subtree.
      child = child->GetNextNonChildNode(aNode);
      continue;
    }
    if (ShadowRoot* shadow = element->GetShadowRoot()) {
      SetDirectionalityOnDescendantsInternal(shadow, aDir, aNotify);
    }

    if (auto* slot = HTMLSlotElement::FromNode(child)) {
      for (const RefPtr<nsINode>& assignedNode : slot->AssignedNodes()) {
        auto* assignedElement = Element::FromNode(*assignedNode);
        if (assignedElement && !IsBoundary(*assignedElement)) {
          assignedElement->SetDirectionality(aDir, aNotify);
          SetDirectionalityOnDescendantsInternal(assignedElement, aDir,
                                                 aNotify);
        }
      }
    }

    element->SetDirectionality(aDir, aNotify);
    child = child->GetNextNode(aNode);
  }
}

// We want the public version of this only to acc
void SetDirectionalityOnDescendants(Element* aElement, Directionality aDir,
                                    bool aNotify) {
  return SetDirectionalityOnDescendantsInternal(aElement, aDir, aNotify);
}

static void ResetAutoDirection(Element* aElement, bool aNotify) {
  MOZ_ASSERT(aElement->HasDirAuto());
  nsTextNode* setByNode =
      WalkDescendantsSetDirectionFromText(aElement, aNotify);
  if (setByNode) {
    setByNode->SetMaySetDirAuto();
  }
  SetDirectionalityOnDescendants(aElement, aElement->GetDirectionality(),
                                 aNotify);
}

/**
 * Walk the parent chain of a text node whose dir attribute has been removed or
 * added and reset the direction of any of its ancestors which have dir=auto and
 * whose directionality is determined by a text node descendant.
 */
void WalkAncestorsResetAutoDirection(Element* aElement, bool aNotify) {
  for (nsIContent* parent = GetParentOrHostOrSlot(aElement);
       parent && parent->NodeOrAncestorHasDirAuto();
       parent = GetParentOrHostOrSlot(parent)) {
    auto* parentElement = Element::FromNode(*parent);
    if (!parentElement || !parentElement->HasDirAuto()) {
      continue;
    }
    nsTextNode* setByNode =
        WalkDescendantsSetDirectionFromText(parentElement, aNotify);
    if (setByNode) {
      setByNode->SetMaySetDirAuto();
    }
    SetDirectionalityOnDescendants(parentElement,
                                   parentElement->GetDirectionality(), aNotify);
    break;
  }
}

static void RecomputeSlottedNodeDirection(HTMLSlotElement& aSlot,
                                          nsINode& aNode) {
  auto* assignedElement = Element::FromNode(aNode);
  if (!assignedElement) {
    return;
  }

  if (assignedElement->HasValidDir() || assignedElement->HasDirAuto()) {
    return;
  }

  // Try to optimize out state changes when possible.
  if (assignedElement->GetDirectionality() == aSlot.GetDirectionality()) {
    return;
  }

  assignedElement->SetDirectionality(aSlot.GetDirectionality(), true);
  SetDirectionalityOnDescendantsInternal(assignedElement,
                                         aSlot.GetDirectionality(), true);
}

void SlotAssignedNodeChanged(HTMLSlotElement* aSlot,
                             nsIContent& aAssignedNode) {
  if (!aSlot) {
    return;
  }

  if (aSlot->NodeOrAncestorHasDirAuto()) {
    // The directionality of the assigned node may impact the directionality of
    // the slot. So recompute everything.
    SlotStateChanged(aSlot, /* aAllAssignedNodesChanged = */ false);
  }

  if (aAssignedNode.GetAssignedSlot() == aSlot) {
    RecomputeSlottedNodeDirection(*aSlot, aAssignedNode);
  }
}

void SlotStateChanged(HTMLSlotElement* aSlot, bool aAllAssignedNodesChanged) {
  if (!aSlot) {
    return;
  }

  Directionality oldDir = aSlot->GetDirectionality();

  if (aSlot->HasDirAuto()) {
    ResetAutoDirection(aSlot, true);
  }

  if (aSlot->NodeOrAncestorHasDirAuto()) {
    WalkAncestorsResetAutoDirection(aSlot, true);
  }

  if (aAllAssignedNodesChanged || oldDir != aSlot->GetDirectionality()) {
    for (nsINode* node : aSlot->AssignedNodes()) {
      RecomputeSlottedNodeDirection(*aSlot, *node);
    }
  }
}

static void SetAncestorHasDirAutoOnDescendants(nsINode* aRoot);

static void MaybeSetAncestorHasDirAutoOnShadowDOM(nsINode* aNode) {
  if (aNode->IsElement()) {
    if (ShadowRoot* sr = aNode->AsElement()->GetShadowRoot()) {
      sr->SetAncestorHasDirAuto();
      SetAncestorHasDirAutoOnDescendants(sr);
    }
  }
}

static void SetAncestorHasDirAutoOnDescendants(nsINode* aRoot) {
  MaybeSetAncestorHasDirAutoOnShadowDOM(aRoot);

  nsIContent* child = aRoot->GetFirstChild();
  while (child) {
    if (child->IsElement() &&
        !AffectsDirectionOfAncestors(child->AsElement())) {
      child = child->GetNextNonChildNode(aRoot);
      continue;
    }

    // If the child is assigned to a slot, it should inherit the state from
    // that.
    if (!child->GetAssignedSlot()) {
      MaybeSetAncestorHasDirAutoOnShadowDOM(child);
      child->SetAncestorHasDirAuto();
      if (auto* slot = HTMLSlotElement::FromNode(child)) {
        const nsTArray<RefPtr<nsINode>>& assignedNodes = slot->AssignedNodes();
        for (uint32_t i = 0; i < assignedNodes.Length(); ++i) {
          assignedNodes[i]->SetAncestorHasDirAuto();
          SetAncestorHasDirAutoOnDescendants(assignedNodes[i]);
        }
      }
    }
    child = child->GetNextNode(aRoot);
  }
}

void WalkDescendantsSetDirAuto(Element* aElement, bool aNotify) {
  // Only test for ParticipatesInAutoDirection -- in other words, if aElement is
  // a <bdi> which is having its dir attribute set to auto (or
  // removed or set to an invalid value, which are equivalent to dir=auto for
  // <bdi>, we *do* want to set AncestorHasDirAuto on its descendants, unlike
  // in SetDirOnBind where we don't propagate AncestorHasDirAuto to a <bdi>
  // being bound to an existing node with dir=auto.
  if (ParticipatesInAutoDirection(aElement) &&
      !aElement->AncestorHasDirAuto()) {
    SetAncestorHasDirAutoOnDescendants(aElement);
  }

  nsTextNode* textNode = WalkDescendantsSetDirectionFromText(aElement, aNotify);
  if (textNode) {
    textNode->SetMaySetDirAuto();
  }
}

void WalkDescendantsClearAncestorDirAuto(nsIContent* aContent) {
  if (aContent->IsElement()) {
    if (ShadowRoot* shadowRoot = aContent->AsElement()->GetShadowRoot()) {
      shadowRoot->ClearAncestorHasDirAuto();
      WalkDescendantsClearAncestorDirAuto(shadowRoot);
    }
  }

  nsIContent* child = aContent->GetFirstChild();
  while (child) {
    if (child->GetAssignedSlot()) {
      // If the child node is assigned to a slot, nodes state is inherited from
      // the slot, not from element's parent.
      child = child->GetNextNonChildNode(aContent);
      continue;
    }
    if (child->IsElement()) {
      if (child->AsElement()->HasDirAuto()) {
        child = child->GetNextNonChildNode(aContent);
        continue;
      }

      if (auto* slot = HTMLSlotElement::FromNode(child)) {
        const nsTArray<RefPtr<nsINode>>& assignedNodes = slot->AssignedNodes();
        for (uint32_t i = 0; i < assignedNodes.Length(); ++i) {
          if (assignedNodes[i]->IsElement()) {
            Element* slottedElement = assignedNodes[i]->AsElement();
            if (slottedElement->HasDirAuto()) {
              continue;
            }
          }

          nsIContent* content = assignedNodes[i]->AsContent();
          content->ClearAncestorHasDirAuto();
          WalkDescendantsClearAncestorDirAuto(content);
        }
      }
    }

    child->ClearAncestorHasDirAuto();
    child = child->GetNextNode(aContent);
  }
}

struct DirAutoElementResult {
  Element* mElement = nullptr;
  // This is false when we hit the top of the ancestor chain without finding a
  // dir=auto element or an element with a fixed direction. This is useful when
  // processing node removals, since we might need to look at the subtree we're
  // removing from.
  bool mAnswerIsDefinitive = false;
};

static DirAutoElementResult FindDirAutoElementFrom(nsIContent* aContent) {
  for (nsIContent* parent = aContent;
       parent && parent->NodeOrAncestorHasDirAuto();
       parent = GetParentOrHostOrSlot(parent)) {
    auto* parentElement = Element::FromNode(*parent);
    if (!parentElement) {
      continue;
    }
    if (!ParticipatesInAutoDirection(parentElement) ||
        parentElement->HasFixedDir()) {
      return {nullptr, true};
    }
    if (parentElement->HasDirAuto()) {
      return {parentElement, true};
    }
  }
  return {nullptr, false};
}

static DirAutoElementResult FindDirAutoElementForText(nsTextNode* aTextNode) {
  MOZ_ASSERT(aTextNode->NodeType() == nsINode::TEXT_NODE,
             "Must be a text node");
  return FindDirAutoElementFrom(GetParentOrHostOrSlot(aTextNode));
}

static DirAutoElementResult SetAncestorDirectionIfAuto(nsTextNode* aTextNode,
                                                       Directionality aDir,
                                                       bool aNotify = true) {
  auto result = FindDirAutoElementForText(aTextNode);
  if (Element* parentElement = result.mElement) {
    if (parentElement->GetDirectionality() == aDir) {
      // If we know that the directionality is already correct, we don't need to
      // reset it. But we might be responsible for the directionality of
      // parentElement.
      MOZ_ASSERT(aDir != Directionality::Unset);
      aTextNode->SetMaySetDirAuto();
    } else {
      // Otherwise recompute the directionality of parentElement.
      ResetAutoDirection(parentElement, aNotify);
    }
  }
  return result;
}

bool TextNodeWillChangeDirection(nsTextNode* aTextNode, Directionality* aOldDir,
                                 uint32_t aOffset) {
  if (!NodeAffectsDirAutoAncestor(aTextNode)) {
    return false;
  }

  // If the change has happened after the first character with strong
  // directionality in the text node, do nothing.
  uint32_t firstStrong;
  *aOldDir = GetDirectionFromText(aTextNode, &firstStrong);
  return (aOffset <= firstStrong);
}

void TextNodeChangedDirection(nsTextNode* aTextNode, Directionality aOldDir,
                              bool aNotify) {
  MOZ_ASSERT(NodeAffectsDirAutoAncestor(aTextNode), "Caller should check");
  Directionality newDir = GetDirectionFromText(aTextNode);
  if (newDir == aOldDir) {
    return;
  }
  // If the old directionality is Unset, we might determine now dir=auto
  // ancestor direction now, even if we don't have the MaySetDirAuto flag.
  //
  // Otherwise we used to have a strong directionality and either no longer
  // does, or it changed. We might need to reset the direction.
  if (aOldDir == Directionality::Unset || aTextNode->MaySetDirAuto()) {
    SetAncestorDirectionIfAuto(aTextNode, newDir, aNotify);
  }
}

void SetDirectionFromNewTextNode(nsTextNode* aTextNode) {
  if (!NodeAffectsDirAutoAncestor(aTextNode)) {
    return;
  }

  nsIContent* parent = GetParentOrHostOrSlot(aTextNode);
  if (parent && parent->NodeOrAncestorHasDirAuto()) {
    aTextNode->SetAncestorHasDirAuto();
  }

  Directionality dir = GetDirectionFromText(aTextNode);
  if (dir != Directionality::Unset) {
    SetAncestorDirectionIfAuto(aTextNode, dir);
  }
}

void ResetDirectionSetByTextNode(nsTextNode* aTextNode,
                                 dom::UnbindContext& aContext) {
  MOZ_ASSERT(!aTextNode->IsInComposedDoc(), "Should be disconnected already");
  if (!aTextNode->MaySetDirAuto()) {
    return;
  }
  auto result = FindDirAutoElementForText(aTextNode);
  if (result.mAnswerIsDefinitive) {
    // The dir=auto element is in our (now detached) subtree. We're done, as
    // nothing really changed for our purposes.
    return;
  }
  MOZ_ASSERT(!result.mElement);
  // The dir=auto element might have been on the element we're unbinding from.
  // In any case, this text node is clearly no longer what determines its
  // directionality.
  aTextNode->ClearMaySetDirAuto();
  auto* unboundFrom =
      nsIContent::FromNodeOrNull(aContext.GetOriginalSubtreeParent());
  if (!unboundFrom || !TextChildrenAffectDirAutoAncestor(unboundFrom)) {
    return;
  }

  Directionality dir = GetDirectionFromText(aTextNode);
  if (dir == Directionality::Unset) {
    return;
  }

  result = FindDirAutoElementFrom(unboundFrom);
  if (!result.mElement || result.mElement->GetDirectionality() != dir) {
    return;
  }
  ResetAutoDirection(result.mElement, /* aNotify = */ true);
}

void SetDirectionalityFromValue(Element* aElement, const nsAString& value,
                                bool aNotify) {
  Directionality dir =
      GetDirectionFromText(value.BeginReading(), value.Length());
  if (dir == Directionality::Unset) {
    dir = Directionality::Ltr;
  }

  if (aElement->GetDirectionality() != dir) {
    aElement->SetDirectionality(dir, aNotify);
  }
}

void OnSetDirAttr(Element* aElement, const nsAttrValue* aNewValue,
                  bool hadValidDir, bool hadDirAuto, bool aNotify) {
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::input, nsGkAtoms::textarea)) {
    return;
  }

  if (aElement->AncestorHasDirAuto()) {
    // The element is a descendant of an element with dir = auto, is having its
    // dir attribute changed. Reset the direction of any of its ancestors whose
    // direction might be determined by a text node descendant
    WalkAncestorsResetAutoDirection(aElement, aNotify);
  } else if (hadDirAuto && !aElement->HasDirAuto()) {
    // The element isn't a descendant of an element with dir = auto, and is
    // having its dir attribute set to something other than auto.
    // Walk the descendant tree and clear the AncestorHasDirAuto flag.
    //
    // N.B: For elements other than <bdi> it would be enough to test that the
    //      current value of dir was "auto" in BeforeSetAttr to know that we
    //      were unsetting dir="auto". For <bdi> things are more complicated,
    //      since it behaves like dir="auto" whenever the dir attribute is
    //      empty or invalid, so we would have to check whether the old value
    //      was not either "ltr" or "rtl", and the new value was either "ltr"
    //      or "rtl". Element::HasDirAuto() encapsulates all that, so doing it
    //      here is simpler.
    WalkDescendantsClearAncestorDirAuto(aElement);
  }

  if (aElement->HasDirAuto()) {
    WalkDescendantsSetDirAuto(aElement, aNotify);
  } else {
    SetDirectionalityOnDescendants(
        aElement, RecomputeDirectionality(aElement, aNotify), aNotify);
  }
}

void SetDirOnBind(Element* aElement, nsIContent* aParent) {
  // Set the AncestorHasDirAuto flag, unless this element shouldn't affect
  // ancestors that have dir=auto
  if (ParticipatesInAutoDirection(aElement) &&
      !aElement->IsHTMLElement(nsGkAtoms::bdi) && aParent &&
      aParent->NodeOrAncestorHasDirAuto()) {
    aElement->SetAncestorHasDirAuto();

    SetAncestorHasDirAutoOnDescendants(aElement);

    if (aElement->GetFirstChild() || aElement->GetShadowRoot()) {
      // We may also need to reset the direction of an ancestor with dir=auto
      WalkAncestorsResetAutoDirection(aElement, true);
    }
  }

  if (!aElement->HasDirAuto()) {
    // if the element doesn't have dir=auto, set its own directionality from
    // the dir attribute or by inheriting from its ancestors.
    RecomputeDirectionality(aElement, false);
  }
}

void ResetDir(Element* aElement) {
  if (!aElement->HasDirAuto()) {
    RecomputeDirectionality(aElement, false);
  }
}

}  // end namespace mozilla
