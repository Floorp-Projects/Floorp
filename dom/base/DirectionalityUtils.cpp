/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  Implementation description from https://etherpad.mozilla.org/dir-auto

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
  Elements with unresolved directionality behave as LTR.

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

  Each text node should maintain a list of elements which have their
  directionality determined by the first strong character of that text node.
  This is useful to make dynamic changes more efficient.  One way to implement
  this is to have a per-document hash table mapping a text node to a set of
  elements.  I'll call this data structure TextNodeDirectionalityMap. The
  algorithm for appending a new text node above needs to update this data
  structure.

  *IMPLEMENTATION NOTE*
  In practice, the implementation uses two per-node properties:

  dirAutoSetBy, which is set on a node with auto-directionality, and points to
  the textnode that contains the strong character which determines the
  directionality of the node.

  textNodeDirectionalityMap, which is set on a text node and points to a hash
  table listing the nodes whose directionality is determined by the text node.

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
  the parent chain in this case.  TextNodeDirectionalityMap needs to be updated
  as appropriate.

  3a. When the dir attribute is set to any valid value on an element that didn't
  have a valid dir attribute before, this means that any descendant of that
  element will not affect the directionality of any of its ancestors. So we need
  to check whether any text node descendants of the element are listed in
  TextNodeDirectionalityMap, and whether the elements whose direction they set
  are ancestors of the element. If so, we need to rerun the downward propagation
  algorithm for those ancestors.

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
  TextNodeDirectionalityMap needs to be updated as appropriate.

  5a. When the dir attribute is removed or set to an invalid value on any
  element (except a bdi element) with the NodeAncestorHasDirAuto flag which
  previously had a valid dir attribute, it might have a text node descendant
  that did not previously affect the directionality of any of its ancestors but
  should now begin to affect them. We run the following algorithm:
  * Walk up the parent chain from the element.
  * For any element that appears in the TextNodeDirectionalityMap, remove the
    element from the map and rerun the downward propagation algorithm
    (see section 3).
  * If we reach an element without either of the NodeHasDirAuto or
    NodeAncestorHasDirAuto flags, abort the parent chain walk.

  6. When an element with @dir=auto is added to the document, we should handle
  it similar to the case 2/3 above.

  7. When an element with NodeHasDirAuto or NodeAncestorHasDirAuto is
  removed from the document, we should handle it similar to the case 4/5 above,
  except that we don't need to handle anything in the child subtree.  We should
  also remove all of the occurrences of that node and its descendants from
  TextNodeDirectionalityMap. (This is the conceptual description of what needs
  to happen but in the implementation UnbindFromTree is going to be called on
  all of the descendants so we don't need to descend into the child subtree).

  8. When the contents of a text node is changed either from script or by the
  user, we need to run the following algorithm:
  * If the change has happened after the first character with strong
  directionality in the text node, do nothing.
  * If the text node is a child of a bdi, script or style element, do nothing.
  * If the text node belongs to a textarea with NodeHasDirAuto, we need to
  update the directionality of the textarea.
  * Grab a list of elements affected by this text node from
  TextNodeDirectionalityMap and re-resolve the directionality of each one of
  them based on the new contents of the text node.
  * If the text node does not exist in TextNodeDirectionalityMap, and it has the
  NodeAncestorHasDirAuto flag set, this could potentially be a text node
  which is going to start affecting the directionality of its parent @dir=auto
  elements. In this case, we need to fall back to the (potentially expensive)
  "upward propagation algorithm".  The TextNodeDirectionalityMap data structure
  needs to be update during this algorithm.
  * If the new contents of the text node do not have any strong characters, and
  the old contents used to, and the text node used to exist in
  TextNodeDirectionalityMap and it has the NodeAncestorHasDirAuto flag set,
  the elements associated with this text node inside TextNodeDirectionalityMap
  will now get their directionality from another text node.  In this case, for
  each element in the list retrieved from TextNodeDirectionalityMap, run the
  downward propagation algorithm (section 3), and remove the text node from
  TextNodeDirectionalityMap.

  9. When a new text node is injected into a document, we need to run the
  following algorithm:
  * If the contents of the text node do not have any characters with strong
  direction, do nothing.
  * If the text node is a child of a bdi, script or style element, do nothing.
  * If the text node is appended to a textarea element with NodeHasDirAuto, we
  need to update the directionality of the textarea.
  * If the text node has NodeAncestorHasDirAuto, we need to run the "upward
  propagation algorithm".  The TextNodeDirectionalityMap data structure needs to
  be update during this algorithm.

  10. When a text node is removed from a document, we need to run the following
  algorithm:
  * If the contents of the text node do not have any characters with strong
  direction, do nothing.
  * If the text node is a child of a bdi, script or style element, do nothing.
  * If the text node is removed from a textarea element with NodeHasDirAuto,
  set the directionality to "ltr". (This is what the spec currently says, but
  I'm filing a spec bug to get it fixed -- the directionality should depend on
  the parent element here.)
  * If the text node has NodeAncestorHasDirAuto, we need to look at the list
  of elements being affected by this text node from TextNodeDirectionalityMap,
  run the "downward propagation algorithm" (section 3) for each one of them,
  while updating TextNodeDirectionalityMap along the way.

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
#include "mozilla/AutoRestore.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsUnicodeProperties.h"
#include "nsTextFragment.h"
#include "nsAttrValue.h"
#include "nsTextNode.h"
#include "nsCheapSets.h"

namespace mozilla {

using mozilla::dom::Element;
using mozilla::dom::HTMLInputElement;
using mozilla::dom::HTMLSlotElement;
using mozilla::dom::ShadowRoot;

static nsIContent* GetParentOrHostOrSlot(
    nsIContent* aContent, bool* aCrossedShadowBoundary = nullptr) {
  if (HTMLSlotElement* slot = aContent->GetAssignedSlot()) {
    if (aCrossedShadowBoundary) {
      *aCrossedShadowBoundary = true;
    }
    return slot;
  }

  nsIContent* parent = aContent->GetParent();
  if (parent) {
    return parent;
  }

  ShadowRoot* sr = ShadowRoot::FromNode(aContent);
  if (sr) {
    if (aCrossedShadowBoundary) {
      *aCrossedShadowBoundary = true;
    }
    return sr->Host();
  }

  return nullptr;
}

static bool AncestorChainCrossesShadowBoundary(nsIContent* aDescendant,
                                               nsIContent* aAncestor) {
  bool crossedShadowBoundary = false;
  nsIContent* content = aDescendant;
  while (content && content != aAncestor) {
    content = GetParentOrHostOrSlot(content, &crossedShadowBoundary);
    if (crossedShadowBoundary) {
      return true;
    }
  }

  return false;
}

/**
 * Returns true if aElement is one of the elements whose text content should not
 * affect its own direction, nor the direction of ancestors with dir=auto.
 *
 * Note that this does not include <bdi>, whose content does affect its own
 * direction when it has dir=auto (which it has by default), so one needs to
 * test for it separately, e.g. with DoesNotAffectDirectionOfAncestors.
 * It *does* include textarea, because even if a textarea has dir=auto, it has
 * unicode-bidi: plaintext and is handled automatically in bidi resolution.
 * It also includes `input`, because it takes the `dir` value from its value
 * attribute, instead of the child nodes.
 */
static bool DoesNotParticipateInAutoDirection(const nsIContent* aContent) {
  mozilla::dom::NodeInfo* nodeInfo = aContent->NodeInfo();
  return ((!aContent->IsHTMLElement() || nodeInfo->Equals(nsGkAtoms::script) ||
           nodeInfo->Equals(nsGkAtoms::style) ||
           nodeInfo->Equals(nsGkAtoms::input) ||
           nodeInfo->Equals(nsGkAtoms::textarea) ||
           aContent->IsInNativeAnonymousSubtree())) &&
         !aContent->IsShadowRoot();
}

/**
 * Returns true if aElement is one of the element whose text content should not
 * affect the direction of ancestors with dir=auto (though it may affect its own
 * direction, e.g. <bdi>)
 */
static bool DoesNotAffectDirectionOfAncestors(const Element* aElement) {
  return (DoesNotParticipateInAutoDirection(aElement) ||
          aElement->IsHTMLElement(nsGkAtoms::bdi) || aElement->HasFixedDir());
}

/**
 * Returns the directionality of a Unicode character
 */
static Directionality GetDirectionFromChar(uint32_t ch) {
  switch (mozilla::unicode::GetBidiCat(ch)) {
    case eCharType_RightToLeft:
    case eCharType_RightToLeftArabic:
      return eDir_RTL;

    case eCharType_LeftToRight:
      return eDir_LTR;

    default:
      return eDir_NotSet;
  }
}

inline static bool NodeAffectsDirAutoAncestor(nsIContent* aTextNode) {
  nsIContent* parent = GetParentOrHostOrSlot(aTextNode);
  return (parent && !DoesNotParticipateInAutoDirection(parent) &&
          parent->NodeOrAncestorHasDirAuto() &&
          !aTextNode->IsInNativeAnonymousSubtree());
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
      if (dir != eDir_NotSet) {
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
  return eDir_NotSet;
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
    if (dir != eDir_NotSet) {
      if (aFirstStrong) {
        *aFirstStrong = current;
      }
      return dir;
    }
  }

  if (aFirstStrong) {
    *aFirstStrong = UINT32_MAX;
  }
  return eDir_NotSet;
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
    nsINode* aRoot, nsINode* aSkip, Directionality* aDirectionality) {
  nsIContent* child = aRoot->GetFirstChild();
  while (child) {
    if ((child->IsElement() &&
         DoesNotAffectDirectionOfAncestors(child->AsElement())) ||
        child->GetAssignedSlot()) {
      child = child->GetNextNonChildNode(aRoot);
      continue;
    }

    if (auto* slot = HTMLSlotElement::FromNode(child)) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes = slot->AssignedNodes();
      for (uint32_t i = 0; i < assignedNodes.Length(); ++i) {
        nsIContent* assignedNode = assignedNodes[i]->AsContent();
        if (assignedNode->NodeType() == nsINode::TEXT_NODE) {
          auto text = static_cast<nsTextNode*>(assignedNode);
          if (assignedNode != aSkip) {
            Directionality textNodeDir = GetDirectionFromText(text);
            if (textNodeDir != eDir_NotSet) {
              *aDirectionality = textNodeDir;
              return text;
            }
          }
        } else if (assignedNode->IsElement() &&
                   !DoesNotAffectDirectionOfAncestors(
                       assignedNode->AsElement())) {
          nsTextNode* text = WalkDescendantsAndGetDirectionFromText(
              assignedNode, aSkip, aDirectionality);
          if (text) {
            return text;
          }
        }
      }
    }

    if (child->NodeType() == nsINode::TEXT_NODE && child != aSkip) {
      auto text = static_cast<nsTextNode*>(child);
      Directionality textNodeDir = GetDirectionFromText(text);
      if (textNodeDir != eDir_NotSet) {
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
 * @param[in] changedNode If we call this method because the content of a text
 *            node is about to change, pass in the changed node, so that we
 *            know not to return it
 * @return the text node containing the character that determined the direction
 */
static nsTextNode* WalkDescendantsSetDirectionFromText(
    Element* aElement, bool aNotify, nsINode* aChangedNode = nullptr) {
  MOZ_ASSERT(aElement, "Must have an element");
  MOZ_ASSERT(aElement->HasDirAuto(), "Element must have dir=auto");

  if (DoesNotParticipateInAutoDirection(aElement)) {
    return nullptr;
  }

  Directionality textNodeDir = eDir_NotSet;

  // Check the text in Shadow DOM.
  if (ShadowRoot* shadowRoot = aElement->GetShadowRoot()) {
    nsTextNode* text = WalkDescendantsAndGetDirectionFromText(
        shadowRoot, aChangedNode, &textNodeDir);
    if (text) {
      aElement->SetDirectionality(textNodeDir, aNotify);
      return text;
    }
  }

  // Check the text in light DOM.
  nsTextNode* text = WalkDescendantsAndGetDirectionFromText(
      aElement, aChangedNode, &textNodeDir);
  if (text) {
    aElement->SetDirectionality(textNodeDir, aNotify);
    return text;
  }

  // We walked all the descendants without finding a text node with strong
  // directional characters. Set the directionality to LTR
  aElement->SetDirectionality(eDir_LTR, aNotify);
  return nullptr;
}

class nsTextNodeDirectionalityMap {
  static void nsTextNodeDirectionalityMapDtor(void* aObject,
                                              nsAtom* aPropertyName,
                                              void* aPropertyValue,
                                              void* aData) {
    nsINode* textNode = static_cast<nsINode*>(aObject);
    textNode->ClearHasTextNodeDirectionalityMap();

    nsTextNodeDirectionalityMap* map =
        reinterpret_cast<nsTextNodeDirectionalityMap*>(aPropertyValue);
    map->EnsureMapIsClear();
    delete map;
  }

 public:
  explicit nsTextNodeDirectionalityMap(nsINode* aTextNode)
      : mElementToBeRemoved(nullptr) {
    MOZ_ASSERT(aTextNode, "Null text node");
    MOZ_COUNT_CTOR(nsTextNodeDirectionalityMap);
    aTextNode->SetProperty(nsGkAtoms::textNodeDirectionalityMap, this,
                           nsTextNodeDirectionalityMapDtor);
    aTextNode->SetHasTextNodeDirectionalityMap();
  }

  MOZ_COUNTED_DTOR(nsTextNodeDirectionalityMap)

  static void nsTextNodeDirectionalityMapPropertyDestructor(
      void* aObject, nsAtom* aProperty, void* aPropertyValue, void* aData) {
    nsTextNode* textNode = static_cast<nsTextNode*>(aPropertyValue);
    nsTextNodeDirectionalityMap* map = GetDirectionalityMap(textNode);
    if (map) {
      map->RemoveEntryForProperty(static_cast<Element*>(aObject));
    }
    NS_RELEASE(textNode);
  }

  void AddEntry(nsTextNode* aTextNode, Element* aElement) {
    if (!mElements.Contains(aElement)) {
      mElements.Put(aElement);
      NS_ADDREF(aTextNode);
      aElement->SetProperty(nsGkAtoms::dirAutoSetBy, aTextNode,
                            nsTextNodeDirectionalityMapPropertyDestructor);
      aElement->SetHasDirAutoSet();
    }
  }

  void RemoveEntry(nsTextNode* aTextNode, Element* aElement) {
    NS_ASSERTION(mElements.Contains(aElement),
                 "element already removed from map");

    mElements.Remove(aElement);
    aElement->ClearHasDirAutoSet();
    aElement->RemoveProperty(nsGkAtoms::dirAutoSetBy);
  }

  void RemoveEntryForProperty(Element* aElement) {
    if (mElementToBeRemoved != aElement) {
      mElements.Remove(aElement);
    }
    aElement->ClearHasDirAutoSet();
  }

 private:
  nsCheapSet<nsPtrHashKey<Element>> mElements;
  // Only used for comparison.
  Element* mElementToBeRemoved;

  static nsTextNodeDirectionalityMap* GetDirectionalityMap(nsINode* aTextNode) {
    MOZ_ASSERT(aTextNode->NodeType() == nsINode::TEXT_NODE,
               "Must be a text node");
    nsTextNodeDirectionalityMap* map = nullptr;

    if (aTextNode->HasTextNodeDirectionalityMap()) {
      map = static_cast<nsTextNodeDirectionalityMap*>(
          aTextNode->GetProperty(nsGkAtoms::textNodeDirectionalityMap));
    }

    return map;
  }

  static nsCheapSetOperator SetNodeDirection(nsPtrHashKey<Element>* aEntry,
                                             void* aDir) {
    aEntry->GetKey()->SetDirectionality(
        *reinterpret_cast<Directionality*>(aDir), true);
    return OpNext;
  }

  struct nsTextNodeDirectionalityMapAndElement {
    nsTextNodeDirectionalityMap* mMap;
    nsCOMPtr<nsINode> mNode;
  };

  static nsCheapSetOperator ResetNodeDirection(nsPtrHashKey<Element>* aEntry,
                                               void* aData) {
    // run the downward propagation algorithm
    // and remove the text node from the map
    nsTextNodeDirectionalityMapAndElement* data =
        static_cast<nsTextNodeDirectionalityMapAndElement*>(aData);
    nsINode* oldTextNode = data->mNode;
    Element* rootNode = aEntry->GetKey();
    nsTextNode* newTextNode = nullptr;
    if (rootNode->GetParentNode() && rootNode->HasDirAuto()) {
      newTextNode =
          WalkDescendantsSetDirectionFromText(rootNode, true, oldTextNode);
    }

    AutoRestore<Element*> restore(data->mMap->mElementToBeRemoved);
    data->mMap->mElementToBeRemoved = rootNode;
    if (newTextNode) {
      nsINode* oldDirAutoSetBy = static_cast<nsTextNode*>(
          rootNode->GetProperty(nsGkAtoms::dirAutoSetBy));
      if (oldDirAutoSetBy == newTextNode) {
        // We're already registered.
        return OpNext;
      }
      nsTextNodeDirectionalityMap::AddEntryToMap(newTextNode, rootNode);
    } else {
      rootNode->ClearHasDirAutoSet();
      rootNode->RemoveProperty(nsGkAtoms::dirAutoSetBy);
    }
    return OpRemove;
  }

  static nsCheapSetOperator TakeEntries(nsPtrHashKey<Element>* aEntry,
                                        void* aData) {
    AutoTArray<Element*, 8>* entries =
        static_cast<AutoTArray<Element*, 8>*>(aData);
    entries->AppendElement(aEntry->GetKey());
    return OpRemove;
  }

 public:
  uint32_t UpdateAutoDirection(Directionality aDir) {
    return mElements.EnumerateEntries(SetNodeDirection, &aDir);
  }

  void ResetAutoDirection(nsINode* aTextNode) {
    nsTextNodeDirectionalityMapAndElement data = {this, aTextNode};
    mElements.EnumerateEntries(ResetNodeDirection, &data);
  }

  void EnsureMapIsClear() {
    AutoRestore<Element*> restore(mElementToBeRemoved);
    AutoTArray<Element*, 8> entries;
    mElements.EnumerateEntries(TakeEntries, &entries);
    for (Element* el : entries) {
      el->ClearHasDirAutoSet();
      el->RemoveProperty(nsGkAtoms::dirAutoSetBy);
    }
  }

  static void RemoveElementFromMap(nsTextNode* aTextNode, Element* aElement) {
    if (aTextNode->HasTextNodeDirectionalityMap()) {
      GetDirectionalityMap(aTextNode)->RemoveEntry(aTextNode, aElement);
    }
  }

  static void AddEntryToMap(nsTextNode* aTextNode, Element* aElement) {
    nsTextNodeDirectionalityMap* map = GetDirectionalityMap(aTextNode);
    if (!map) {
      map = new nsTextNodeDirectionalityMap(aTextNode);
    }

    map->AddEntry(aTextNode, aElement);
  }

  static uint32_t UpdateTextNodeDirection(nsINode* aTextNode,
                                          Directionality aDir) {
    MOZ_ASSERT(aTextNode->HasTextNodeDirectionalityMap(),
               "Map missing in UpdateTextNodeDirection");
    return GetDirectionalityMap(aTextNode)->UpdateAutoDirection(aDir);
  }

  static void ResetTextNodeDirection(nsTextNode* aTextNode,
                                     nsTextNode* aChangedTextNode) {
    MOZ_ASSERT(aTextNode->HasTextNodeDirectionalityMap(),
               "Map missing in ResetTextNodeDirection");
    RefPtr<nsTextNode> textNode = aTextNode;
    GetDirectionalityMap(textNode)->ResetAutoDirection(aChangedTextNode);
  }

  static void EnsureMapIsClearFor(nsINode* aTextNode) {
    if (aTextNode->HasTextNodeDirectionalityMap()) {
      GetDirectionalityMap(aTextNode)->EnsureMapIsClear();
    }
  }
};

Directionality RecomputeDirectionality(Element* aElement, bool aNotify) {
  MOZ_ASSERT(!aElement->HasDirAuto(),
             "RecomputeDirectionality called with dir=auto");

  if (aElement->HasValidDir()) {
    return aElement->GetDirectionality();
  }

  Directionality dir = eDir_LTR;
  if (nsIContent* parent = GetParentOrHostOrSlot(aElement)) {
    if (ShadowRoot* shadow = ShadowRoot::FromNode(parent)) {
      parent = shadow->GetHost();
    }

    if (parent && parent->IsElement()) {
      // If the node doesn't have an explicit dir attribute with a valid value,
      // the directionality is the same as the parent element (but don't
      // propagate the parent directionality if it isn't set yet).
      Directionality parentDir = parent->AsElement()->GetDirectionality();
      if (parentDir != eDir_NotSet) {
        dir = parentDir;
      }
    }
  }

  aElement->SetDirectionality(dir, aNotify);
  return dir;
}

static void SetDirectionalityOnDescendantsInternal(nsINode* aNode,
                                                   Directionality aDir,
                                                   bool aNotify) {
  if (Element* element = Element::FromNode(aNode)) {
    if (ShadowRoot* shadow = element->GetShadowRoot()) {
      SetDirectionalityOnDescendantsInternal(shadow, aDir, aNotify);
    }
  }

  for (nsIContent* child = aNode->GetFirstChild(); child;) {
    if (!child->IsElement()) {
      child = child->GetNextNode(aNode);
      continue;
    }

    Element* element = child->AsElement();
    if (element->HasValidDir() || element->HasDirAuto() ||
        element->GetAssignedSlot()) {
      child = child->GetNextNonChildNode(aNode);
      continue;
    }
    if (ShadowRoot* shadow = element->GetShadowRoot()) {
      SetDirectionalityOnDescendantsInternal(shadow, aDir, aNotify);
    }

    if (auto* slot = HTMLSlotElement::FromNode(child)) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes = slot->AssignedNodes();
      for (uint32_t i = 0; i < assignedNodes.Length(); ++i) {
        nsINode* node = assignedNodes[i];
        Element* assignedElement =
            node->IsElement() ? node->AsElement() : nullptr;
        if (assignedElement && !assignedElement->HasValidDir() &&
            !assignedElement->HasDirAuto()) {
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
  if (aElement->HasDirAutoSet()) {
    // If the parent has the DirAutoSet flag, its direction is determined by
    // some text node descendant.
    // Remove it from the map and reset its direction by the downward
    // propagation algorithm
    nsTextNode* setByNode = static_cast<nsTextNode*>(
        aElement->GetProperty(nsGkAtoms::dirAutoSetBy));
    if (setByNode) {
      nsTextNodeDirectionalityMap::RemoveElementFromMap(setByNode, aElement);
    }
  }

  if (aElement->HasDirAuto()) {
    nsTextNode* setByNode =
        WalkDescendantsSetDirectionFromText(aElement, aNotify);
    if (setByNode) {
      nsTextNodeDirectionalityMap::AddEntryToMap(setByNode, aElement);
    }
    SetDirectionalityOnDescendants(aElement, aElement->GetDirectionality(),
                                   aNotify);
  }
}

/**
 * Walk the parent chain of a text node whose dir attribute has been removed and
 * reset the direction of any of its ancestors which have dir=auto and whose
 * directionality is determined by a text node descendant.
 */
void WalkAncestorsResetAutoDirection(Element* aElement, bool aNotify) {
  nsTextNode* setByNode;
  nsIContent* parent = GetParentOrHostOrSlot(aElement);
  while (parent && parent->NodeOrAncestorHasDirAuto()) {
    if (!parent->IsElement()) {
      parent = GetParentOrHostOrSlot(parent);
      continue;
    }

    Element* parentElement = parent->AsElement();
    if (parent->HasDirAutoSet()) {
      // If the parent has the DirAutoSet flag, its direction is determined by
      // some text node descendant.
      // Remove it from the map and reset its direction by the downward
      // propagation algorithm
      setByNode = static_cast<nsTextNode*>(
          parent->GetProperty(nsGkAtoms::dirAutoSetBy));
      if (setByNode) {
        nsTextNodeDirectionalityMap::RemoveElementFromMap(setByNode,
                                                          parentElement);
      }
    }
    if (parentElement->HasDirAuto()) {
      setByNode = WalkDescendantsSetDirectionFromText(parentElement, aNotify);
      if (setByNode) {
        nsTextNodeDirectionalityMap::AddEntryToMap(setByNode, parentElement);
      }
      SetDirectionalityOnDescendants(
          parentElement, parentElement->GetDirectionality(), aNotify);
      break;
    }
    parent = GetParentOrHostOrSlot(parent);
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

void WalkDescendantsResetAutoDirection(Element* aElement) {
  nsIContent* child = aElement->GetFirstChild();
  while (child) {
    if (child->IsElement() && child->AsElement()->HasDirAuto()) {
      child = child->GetNextNonChildNode(aElement);
      continue;
    }

    if (child->NodeType() == nsINode::TEXT_NODE &&
        child->HasTextNodeDirectionalityMap()) {
      nsTextNodeDirectionalityMap::ResetTextNodeDirection(
          static_cast<nsTextNode*>(child), nullptr);
      // Don't call nsTextNodeDirectionalityMap::EnsureMapIsClearFor(child)
      // since ResetTextNodeDirection may have kept elements in child's
      // DirectionalityMap.
    }
    child = child->GetNextNode(aElement);
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
        DoesNotAffectDirectionOfAncestors(child->AsElement())) {
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
  // Only test for DoesNotParticipateInAutoDirection -- in other words, if
  // aElement is a <bdi> which is having its dir attribute set to auto (or
  // removed or set to an invalid value, which are equivalent to dir=auto for
  // <bdi>, we *do* want to set AncestorHasDirAuto on its descendants, unlike
  // in SetDirOnBind where we don't propagate AncestorHasDirAuto to a <bdi>
  // being bound to an existing node with dir=auto.
  if (!DoesNotParticipateInAutoDirection(aElement) &&
      !aElement->AncestorHasDirAuto()) {
    SetAncestorHasDirAutoOnDescendants(aElement);
  }

  nsTextNode* textNode = WalkDescendantsSetDirectionFromText(aElement, aNotify);
  if (textNode) {
    nsTextNodeDirectionalityMap::AddEntryToMap(textNode, aElement);
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

void SetAncestorDirectionIfAuto(nsTextNode* aTextNode, Directionality aDir,
                                bool aNotify = true) {
  MOZ_ASSERT(aTextNode->NodeType() == nsINode::TEXT_NODE,
             "Must be a text node");

  bool crossedShadowBoundary = false;
  nsIContent* parent = GetParentOrHostOrSlot(aTextNode, &crossedShadowBoundary);
  while (parent && parent->NodeOrAncestorHasDirAuto()) {
    if (!parent->IsElement()) {
      parent = GetParentOrHostOrSlot(parent, &crossedShadowBoundary);
      continue;
    }

    Element* parentElement = parent->AsElement();
    if (DoesNotParticipateInAutoDirection(parentElement) ||
        parentElement->HasFixedDir()) {
      break;
    }

    if (parentElement->HasDirAuto()) {
      bool resetDirection = false;
      nsTextNode* directionWasSetByTextNode = static_cast<nsTextNode*>(
          parent->GetProperty(nsGkAtoms::dirAutoSetBy));

      if (!parent->HasDirAutoSet()) {
        // Fast path if parent's direction is not yet set by any descendant
        MOZ_ASSERT(!directionWasSetByTextNode,
                   "dirAutoSetBy property should be null");
        resetDirection = true;
      } else {
        // If parent's direction is already set, we need to know if
        // aTextNode is before or after the text node that had set it.
        // We will walk parent's descendants in tree order starting from
        // aTextNode to optimize for the most common case where text nodes are
        // being appended to tree.
        if (!directionWasSetByTextNode) {
          resetDirection = true;
        } else if (directionWasSetByTextNode != aTextNode) {
          if (crossedShadowBoundary || AncestorChainCrossesShadowBoundary(
                                           directionWasSetByTextNode, parent)) {
            // Need to take the slow path when the path from either the old or
            // new text node to the dir=auto element crosses shadow boundary.
            ResetAutoDirection(parentElement, aNotify);
            return;
          }

          nsIContent* child = aTextNode->GetNextNode(parent);
          while (child) {
            if (child->IsElement() &&
                DoesNotAffectDirectionOfAncestors(child->AsElement())) {
              child = child->GetNextNonChildNode(parent);
              continue;
            }

            if (child == directionWasSetByTextNode) {
              // we found the node that set the element's direction after our
              // text node, so we need to reset the direction
              resetDirection = true;
              break;
            }

            child = child->GetNextNode(parent);
          }
        }
      }

      if (resetDirection) {
        if (directionWasSetByTextNode) {
          nsTextNodeDirectionalityMap::RemoveElementFromMap(
              directionWasSetByTextNode, parentElement);
        }
        parentElement->SetDirectionality(aDir, aNotify);
        nsTextNodeDirectionalityMap::AddEntryToMap(aTextNode, parentElement);
        SetDirectionalityOnDescendants(parentElement, aDir, aNotify);
      }

      // Since we found an element with dir=auto, we can stop walking the
      // parent chain: none of its ancestors will have their direction set by
      // any of its descendants.
      return;
    }
    parent = GetParentOrHostOrSlot(parent, &crossedShadowBoundary);
  }
}

bool TextNodeWillChangeDirection(nsTextNode* aTextNode, Directionality* aOldDir,
                                 uint32_t aOffset) {
  if (!NodeAffectsDirAutoAncestor(aTextNode)) {
    nsTextNodeDirectionalityMap::EnsureMapIsClearFor(aTextNode);
    return false;
  }

  uint32_t firstStrong;
  *aOldDir = GetDirectionFromText(aTextNode, &firstStrong);
  return (aOffset <= firstStrong);
}

void TextNodeChangedDirection(nsTextNode* aTextNode, Directionality aOldDir,
                              bool aNotify) {
  Directionality newDir = GetDirectionFromText(aTextNode);
  if (newDir == eDir_NotSet) {
    if (aOldDir != eDir_NotSet && aTextNode->HasTextNodeDirectionalityMap()) {
      // This node used to have a strong directional character but no
      // longer does. ResetTextNodeDirection() will re-resolve the
      // directionality of any elements whose directionality was
      // determined by this node.
      nsTextNodeDirectionalityMap::ResetTextNodeDirection(aTextNode, aTextNode);
    }
  } else {
    // This node has a strong directional character. If it has a
    // TextNodeDirectionalityMap property, it already determines the
    // directionality of some element(s), so call UpdateTextNodeDirection to
    // reresolve their directionality. If it has no map, or if
    // UpdateTextNodeDirection returns zero, indicating that the map is
    // empty, call SetAncestorDirectionIfAuto to find ancestor elements
    // which should have their directionality determined by this node.
    if (aTextNode->HasTextNodeDirectionalityMap() &&
        nsTextNodeDirectionalityMap::UpdateTextNodeDirection(aTextNode,
                                                             newDir)) {
      return;
    }
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
  if (dir != eDir_NotSet) {
    SetAncestorDirectionIfAuto(aTextNode, dir);
  }
}

void ResetDirectionSetByTextNode(nsTextNode* aTextNode) {
  if (!NodeAffectsDirAutoAncestor(aTextNode)) {
    nsTextNodeDirectionalityMap::EnsureMapIsClearFor(aTextNode);
    return;
  }

  Directionality dir = GetDirectionFromText(aTextNode);
  if (dir != eDir_NotSet && aTextNode->HasTextNodeDirectionalityMap()) {
    nsTextNodeDirectionalityMap::ResetTextNodeDirection(aTextNode, aTextNode);
  }
}

void SetDirectionalityFromValue(Element* aElement, const nsAString& value,
                                bool aNotify) {
  Directionality dir =
      GetDirectionFromText(value.BeginReading(), value.Length());
  if (dir == eDir_NotSet) {
    dir = eDir_LTR;
  }

  aElement->SetDirectionality(dir, aNotify);
}

void OnSetDirAttr(Element* aElement, const nsAttrValue* aNewValue,
                  bool hadValidDir, bool hadDirAuto, bool aNotify) {
  if (aElement->IsHTMLElement(nsGkAtoms::input)) {
    return;
  }

  if (aElement->AncestorHasDirAuto()) {
    if (!hadValidDir) {
      // The element is a descendant of an element with dir = auto, is
      // having its dir attribute set, and previously didn't have a valid dir
      // attribute.
      // Check whether any of its text node descendants determine the
      // direction of any of its ancestors, and redetermine their direction
      WalkDescendantsResetAutoDirection(aElement);
    } else if (!aElement->HasValidDir()) {
      // The element is a descendant of an element with dir = auto and is
      // having its dir attribute removed or set to an invalid value.
      // Reset the direction of any of its ancestors whose direction is
      // determined by a text node descendant
      WalkAncestorsResetAutoDirection(aElement, aNotify);
    }
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
    if (aElement->HasDirAutoSet()) {
      nsTextNode* setByNode = static_cast<nsTextNode*>(
          aElement->GetProperty(nsGkAtoms::dirAutoSetBy));
      nsTextNodeDirectionalityMap::RemoveElementFromMap(setByNode, aElement);
    }
    SetDirectionalityOnDescendants(
        aElement, RecomputeDirectionality(aElement, aNotify), aNotify);
  }
}

void SetDirOnBind(Element* aElement, nsIContent* aParent) {
  // Set the AncestorHasDirAuto flag, unless this element shouldn't affect
  // ancestors that have dir=auto
  if (!DoesNotParticipateInAutoDirection(aElement) &&
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
  if (aElement->HasDirAutoSet()) {
    nsTextNode* setByNode = static_cast<nsTextNode*>(
        aElement->GetProperty(nsGkAtoms::dirAutoSetBy));
    nsTextNodeDirectionalityMap::RemoveElementFromMap(setByNode, aElement);
  }

  if (!aElement->HasDirAuto()) {
    RecomputeDirectionality(aElement, false);
  }
}

}  // end namespace mozilla
