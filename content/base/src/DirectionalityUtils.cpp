/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
  in http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#the-dir-attribute
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
  resolve the directionality based on the directionality of the value of the @dir
  attribute on element itself or its parent element.

  5. When the dir attribute is changed from auto to something else (including the
  case where it gets removed) on any element except case 4 above and the bdi
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
  previously had a valid dir attribute, it might have a text node descendant that
  did not previously affect the directionality of any of its ancestors but should
  now begin to affect them.
  We run the following algorithm:
  * Walk up the parent chain from the element.
  * For any element that appears in the TextNodeDirectionalityMap, remove the
    element from the map and rerun the downward propagation algorithm
    (see section 3).
  * If we reach an element without either of the NodeHasDirAuto or
    NodeAncestorHasDirAuto flags, abort the parent chain walk.

  6. When an element with @dir=auto is added to the document, we should handle it
  similar to the case 2/3 above.

  7. When an element with NodeHasDirAuto or NodeAncestorHasDirAuto is
  removed from the document, we should handle it similar to the case 4/5 above,
  except that we don't need to handle anything in the child subtree.  We should
  also remove all of the occurrences of that node and its descendants from
  TextNodeDirectionalityMap. (This is the conceptual description of what needs to
  happen but in the implementation UnbindFromTree is going to be called on all of
  the descendants so we don't need to descend into the child subtree).

  8. When the contents of a text node is changed either from script or by the
  user, we need to run the following algorithm:
  * If the change has happened after the first character with strong
  directionality in the text node, do nothing.
  * If the text node is a child of a bdi, script or style element, do nothing.
  * If the text node belongs to a textarea with NodeHasDirAuto, we need to
  update the directionality of the textarea.
  * Grab a list of elements affected by this text node from
  TextNodeDirectionalityMap and re-resolve the directionality of each one of them
  based on the new contents of the text node.
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
  set the directionality to "ltr". (This is what the spec currently says, but I'm
  filing a spec bug to get it fixed -- the directionality should depend on the
  parent element here.)
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
  When the contents of a text node change, nsGenericDOMDataNode::SetTextInternal
  gets called.
  */

#include "mozilla/dom/DirectionalityUtils.h"
#include "nsINode.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMHTMLDocument.h"
#include "nsUnicodeProperties.h"
#include "nsTextFragment.h"
#include "nsAttrValue.h"
#include "nsContentUtils.h"
#include "nsTextNode.h"
#include "nsCheapSets.h"

namespace mozilla {

typedef mozilla::dom::Element Element;

/**
 * Returns true if aNode is one of the elements whose text content should not
 * affect its own direction, nor the direction of ancestors with dir=auto.
 *
 * Note that this does not include <bdi>, whose content does affect its own
 * direction when it has dir=auto (which it has by default), so one needs to
 * test for it separately.
 * It *does* include textarea, because even if a textarea has dir=auto, it has
 * unicode-bidi: plaintext and is handled automatically in bidi resolution.
 */
static bool
DoesNotParticipateInAutoDirection(const Element* aElement)
{
  nsINodeInfo* nodeInfo = aElement->NodeInfo();
  return (aElement->IsHTML() &&
          (nodeInfo->Equals(nsGkAtoms::script) ||
           nodeInfo->Equals(nsGkAtoms::style) ||
           nodeInfo->Equals(nsGkAtoms::textarea)));
}

/**
 * Returns the directionality of a Unicode character
 */
static Directionality
GetDirectionFromChar(uint32_t ch)
{
  switch(mozilla::unicode::GetBidiCat(ch)) {
    case eCharType_RightToLeft:
    case eCharType_RightToLeftArabic:
      return eDir_RTL;

    case eCharType_LeftToRight:
      return eDir_LTR;

    default:
      return eDir_NotSet;
  }
}

inline static bool NodeAffectsDirAutoAncestor(nsINode* aTextNode)
{
  Element* parent = aTextNode->GetElementParent();
  return (parent &&
          !DoesNotParticipateInAutoDirection(parent) &&
          parent->NodeOrAncestorHasDirAuto());
}

/**
 * Various methods for returning the directionality of a string using the
 * first-strong algorithm defined in http://unicode.org/reports/tr9/#P2
 *
 * @param[out] aFirstStrong the offset to the first character in the string with
 *             strong directionality, or UINT32_MAX if there is none (return
               value is eDir_NotSet).
 * @return the directionality of the string
 */
static Directionality
GetDirectionFromText(const PRUnichar* aText, const uint32_t aLength,
                     uint32_t* aFirstStrong = nullptr)
{
  const PRUnichar* start = aText;
  const PRUnichar* end = aText + aLength;

  while (start < end) {
    uint32_t current = start - aText;
    uint32_t ch = *start++;

    if (NS_IS_HIGH_SURROGATE(ch) &&
        start < end &&
        NS_IS_LOW_SURROGATE(*start)) {
      ch = SURROGATE_TO_UCS4(ch, *start++);
    }

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

static Directionality
GetDirectionFromText(const char* aText, const uint32_t aLength,
                        uint32_t* aFirstStrong = nullptr)
{
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

static Directionality
GetDirectionFromText(const nsTextFragment* aFrag,
                     uint32_t* aFirstStrong = nullptr)
{
  if (aFrag->Is2b()) {
    return GetDirectionFromText(aFrag->Get2b(), aFrag->GetLength(),
                                   aFirstStrong);
  }

  return GetDirectionFromText(aFrag->Get1b(), aFrag->GetLength(),
                                 aFirstStrong);
}

/**
 * Set the directionality of a node with dir=auto as defined in
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#the-directionality
 *
 * @param[in] aStartAfterNode as an optimization, a caller may pass in a node
 *            from which to begin walking the descendants of aElement, if it is
 *            known that all text nodes before this node do not contain any
 *            strong directional characters
 * @return the text node containing the character that determined the direction
 */
static nsINode*
WalkDescendantsSetDirectionFromText(Element* aElement, bool aNotify = true,
                                       nsINode* aStartAfterNode = nullptr)
{
  MOZ_ASSERT(aElement, "aElement is null");
  if (DoesNotParticipateInAutoDirection(aElement)) {
    return nullptr;
  }

  nsIContent* child;
  if (aStartAfterNode &&
      nsContentUtils::ContentIsDescendantOf(aStartAfterNode, aElement)) {
    nsIContent* firstNode = aStartAfterNode->GetNextNode(aElement);

#ifdef DEBUG
    // In debug builds, assert that aStartAfterNode is correctly set by checking
    // that text node descendants of elements up to aStartAfterNode don't have
    // any strong directional characters
    child = aElement->GetFirstChild();
    while (child && child != firstNode) {
      // Skip over nodes whose text node descendants don't affect directionality
      // of their ancestors
      if (child->IsElement() &&
          (DoesNotParticipateInAutoDirection(child->AsElement()) ||
           child->NodeInfo()->Equals(nsGkAtoms::bdi) ||
           child->HasFixedDir())) {
        child = child->GetNextNonChildNode(aElement);
        continue;
      }

      if (child->NodeType() == nsIDOMNode::TEXT_NODE) {
        MOZ_ASSERT(GetDirectionFromText(child->GetText()) == eDir_NotSet,
                   "Strong directional characters before aStartAfterNode");
      }
      child = child->GetNextNode(aElement);
    }
#else
    child = firstNode;
#endif
  } else {
    child = aElement->GetFirstChild();
  }

  while (child) {
    if (child->IsElement() &&
        (DoesNotParticipateInAutoDirection(child->AsElement()) ||
         child->NodeInfo()->Equals(nsGkAtoms::bdi) ||
         child->HasFixedDir())) {
      child = child->GetNextNonChildNode(aElement);
      continue;
    }

    if (child->NodeType() == nsIDOMNode::TEXT_NODE) {
      Directionality textNodeDir = GetDirectionFromText(child->GetText());
      if (textNodeDir != eDir_NotSet) {
        // We found a descendant text node with strong directional characters.
        // Set the directionality of aElement to the corresponding value.
        aElement->SetDirectionality(textNodeDir, aNotify);
        return child;
      }
    }
    child = child->GetNextNode(aElement);
  }

  // We walked all the descendants without finding a text node with strong
  // directional characters. Set the directionality to LTR
  aElement->SetDirectionality(eDir_LTR, aNotify);
  return nullptr;
}

class nsTextNodeDirectionalityMap
{
  static void
  nsTextNodeDirectionalityMapDtor(void *aObject, nsIAtom* aPropertyName,
                                  void *aPropertyValue, void* aData)
  {
    nsTextNodeDirectionalityMap* map =
      reinterpret_cast<nsTextNodeDirectionalityMap * >(aPropertyValue);
    delete map;
  }

public:
  nsTextNodeDirectionalityMap(nsINode* aTextNode)
  {
    MOZ_ASSERT(aTextNode, "Null text node");
    MOZ_COUNT_CTOR(nsTextNodeDirectionalityMap);
    aTextNode->SetProperty(nsGkAtoms::textNodeDirectionalityMap, this,
                           nsTextNodeDirectionalityMapDtor);
    aTextNode->SetHasTextNodeDirectionalityMap();
  }

  ~nsTextNodeDirectionalityMap()
  {
    MOZ_COUNT_DTOR(nsTextNodeDirectionalityMap);
  }

  void AddEntry(nsINode* aTextNode, Element* aElement)
  {
    if (!mElements.Contains(aElement)) {
      mElements.Put(aElement);
      aElement->SetProperty(nsGkAtoms::dirAutoSetBy, aTextNode);
      aElement->SetHasDirAutoSet();
    }
  }

  void RemoveEntry(nsINode* aTextNode, Element* aElement)
  {
    if (mElements.Contains(aElement)) {
      mElements.Remove(aElement);

      aElement->ClearHasDirAutoSet();
      aElement->UnsetProperty(nsGkAtoms::dirAutoSetBy);
    }
  }

private:
  nsCheapSet<nsPtrHashKey<Element> > mElements;

  static nsTextNodeDirectionalityMap* GetDirectionalityMap(nsINode* aTextNode)
  {
    MOZ_ASSERT(aTextNode->NodeType() == nsIDOMNode::TEXT_NODE,
               "Must be a text node");
    nsTextNodeDirectionalityMap* map = nullptr;

    if (aTextNode->HasTextNodeDirectionalityMap()) {
      map = static_cast<nsTextNodeDirectionalityMap * >
        (aTextNode->GetProperty(nsGkAtoms::textNodeDirectionalityMap));
    }

    return map;
  }

  static PLDHashOperator SetNodeDirection(nsPtrHashKey<Element>* aEntry, void* aDir)
  {
    MOZ_ASSERT(aEntry->GetKey()->IsElement(), "Must be an Element");
    aEntry->GetKey()->SetDirectionality(*reinterpret_cast<Directionality*>(aDir),
                                        true);
    return PL_DHASH_NEXT;
  }

  static PLDHashOperator ResetNodeDirection(nsPtrHashKey<Element>* aEntry, void* aData)
  {
    MOZ_ASSERT(aEntry->GetKey()->IsElement(), "Must be an Element");
    // run the downward propagation algorithm
    // and remove the text node from the map
    nsINode* startAfterNode = static_cast<Element*>(aData);
    Element* rootNode = aEntry->GetKey();
    nsINode* textNode = WalkDescendantsSetDirectionFromText(rootNode, true,
                                                            startAfterNode);
    if (textNode) {
      nsTextNodeDirectionalityMap::AddEntryToMap(textNode, rootNode);
    } else {
      rootNode->ClearHasDirAutoSet();
      rootNode->UnsetProperty(nsGkAtoms::dirAutoSetBy);
    }
    return PL_DHASH_REMOVE;
  }

public:
  void UpdateAutoDirection(Directionality aDir)
  {
    mElements.EnumerateEntries(SetNodeDirection, &aDir);
  }

  void ResetAutoDirection(nsINode* aTextNode, nsINode* aStartAfterNode)
  {
    mElements.EnumerateEntries(ResetNodeDirection, aStartAfterNode);
  }

  static void RemoveElementFromMap(nsINode* aTextNode, Element* aElement)
  {
    if (aTextNode->HasTextNodeDirectionalityMap()) {
      GetDirectionalityMap(aTextNode)->RemoveEntry(aTextNode, aElement);
    }
  }

  static void AddEntryToMap(nsINode* aTextNode, Element* aElement)
  {
    nsTextNodeDirectionalityMap* map = GetDirectionalityMap(aTextNode);
    if (!map) {
      map = new nsTextNodeDirectionalityMap(aTextNode);
    }

    map->AddEntry(aTextNode, aElement);
  }

  static void UpdateTextNodeDirection(nsINode* aTextNode, Directionality aDir)
  {
    MOZ_ASSERT(aTextNode->HasTextNodeDirectionalityMap(),
               "Map missing in UpdateTextNodeDirection");
    GetDirectionalityMap(aTextNode)->UpdateAutoDirection(aDir);
  }

  static void ResetTextNodeDirection(nsINode* aTextNode,
                                     nsINode* aStartAfterNode = nullptr)
  {
    MOZ_ASSERT(aTextNode->HasTextNodeDirectionalityMap(),
               "Map missing in ResetTextNodeDirection");
    GetDirectionalityMap(aTextNode)->ResetAutoDirection(aTextNode,
                                                        aStartAfterNode);
  }
};

Directionality
RecomputeDirectionality(Element* aElement, bool aNotify)
{
  MOZ_ASSERT(!aElement->HasDirAuto(),
             "RecomputeDirectionality called with dir=auto");
  if (aElement->HasDirAutoSet()) {
    nsINode* setByNode =
      static_cast<nsINode*>(aElement->GetProperty(nsGkAtoms::dirAutoSetBy));
    if (setByNode) {
      nsTextNodeDirectionalityMap::RemoveElementFromMap(setByNode, aElement);
    }
  }

  Directionality dir = eDir_LTR;

  if (aElement->HasValidDir()) {
    dir = aElement->GetDirectionality();
  } else {
    Element* parent = aElement->GetElementParent();
    if (parent) {
      // If the element doesn't have an explicit dir attribute with a valid
      // value, the directionality is the same as the parent element (but
      // don't propagate the parent directionality if it isn't set yet).
      Directionality parentDir = parent->GetDirectionality();
      if (parentDir != eDir_NotSet) {
        dir = parentDir;
      }
    } else {
      // If there is no parent element, the directionality is the same as the
      // document direction.
      Directionality documentDir =
        aElement->OwnerDoc()->GetDocumentDirectionality();
      if (documentDir != eDir_NotSet) {
        dir = documentDir;
      }
    }

    aElement->SetDirectionality(dir, aNotify);
  }
  return dir;
}

void
SetDirectionalityOnDescendants(Element* aElement, Directionality aDir,
                               bool aNotify)
{
  for (nsIContent* child = aElement->GetFirstChild(); child; ) {
    if (!child->IsElement()) {
      child = child->GetNextNode(aElement);
      continue;
    }

    Element* element = child->AsElement();
    if (element->HasValidDir() || element->HasDirAuto()) {
      child = child->GetNextNonChildNode(aElement);
      continue;
    }
    element->SetDirectionality(aDir, aNotify);
    child = child->GetNextNode(aElement);
  }
}

void
WalkAncestorsResetAutoDirection(Element* aElement, bool aNotify)
{
  nsINode* setByNode;
  Element* parent = aElement->GetElementParent();

  while (parent && parent->NodeOrAncestorHasDirAuto()) {
    if (parent->HasDirAutoSet()) {
      // If the parent has the DirAutoSet flag, its direction is determined by
      // some text node descendant.
      // Remove it from the map and reset its direction by the downward
      // propagation algorithm
      setByNode =
        static_cast<nsINode*>(parent->GetProperty(nsGkAtoms::dirAutoSetBy));
      if (setByNode) {
        nsTextNodeDirectionalityMap::RemoveElementFromMap(setByNode, parent);
      }
    }
    if (parent->HasDirAuto()) {
      setByNode = WalkDescendantsSetDirectionFromText(parent, aNotify);
      if (setByNode) {
        nsTextNodeDirectionalityMap::AddEntryToMap(setByNode, parent);
      }
      break;
    }
    parent = parent->GetElementParent();
  }
}

void
WalkDescendantsResetAutoDirection(Element* aElement)
{
  nsIContent* child = aElement->GetFirstChild();
  while (child) {
    if (child->HasDirAuto()) {
      child = child->GetNextNonChildNode(aElement);
      continue;
    }

    if (child->HasTextNodeDirectionalityMap()) {
      nsTextNodeDirectionalityMap::ResetTextNodeDirection(child, child);
    }
    child = child->GetNextNode(aElement);
  }
}

void
WalkDescendantsSetDirAuto(Element* aElement, bool aNotify)
{
  bool setAncestorDirAutoFlag =
#ifdef DEBUG
    true;
#else
    !aElement->AncestorHasDirAuto();
#endif

  if (setAncestorDirAutoFlag) {
    nsIContent* child = aElement->GetFirstChild();
    while (child) {
      MOZ_ASSERT(!aElement->AncestorHasDirAuto() ||
                 child->AncestorHasDirAuto(),
                 "AncestorHasDirAuto set on node but not its children");
      child->SetHasDirAuto();
      child = child->GetNextNode(aElement);
    }
  }

  nsINode* textNode = WalkDescendantsSetDirectionFromText(aElement, aNotify);
  if (textNode) {
    nsTextNodeDirectionalityMap::AddEntryToMap(textNode, aElement);
  }
}

void
WalkDescendantsClearAncestorDirAuto(Element* aElement)
{
  nsIContent* child = aElement->GetFirstChild();
  while (child) {
    if (child->HasDirAuto()) {
      child = child->GetNextNonChildNode(aElement);
      continue;
    }

    child->ClearAncestorHasDirAuto();
    child = child->GetNextNode(aElement);
  }
}

void SetAncestorDirectionIfAuto(nsINode* aTextNode, Directionality aDir,
                                bool aNotify = true)
{
  MOZ_ASSERT(aTextNode->NodeType() == nsIDOMNode::TEXT_NODE,
             "Must be a text node");

  Element* parent = aTextNode->GetElementParent();
  while (parent && parent->NodeOrAncestorHasDirAuto()) {
    if (DoesNotParticipateInAutoDirection(parent) || parent->HasFixedDir()) {
      break;
    }

    if (parent->HasDirAuto()) {
      bool resetDirection = false;

      if (!parent->HasDirAutoSet()) {
        // Fast path if parent's direction is not yet set by any descendant
        resetDirection = true;
      } else {
        // If parent's direction is already set, we need to know if
        // aTextNode is before or after the text node that had set it.
        // We will walk parent's descendants in tree order starting from
        // aTextNode to optimize for the most common case where text nodes are
        // being appended to tree.
        nsINode* directionWasSetByTextNode =
          static_cast<nsINode*>(parent->GetProperty(nsGkAtoms::dirAutoSetBy));
        if (!directionWasSetByTextNode) {
          resetDirection = true;
        } else if (directionWasSetByTextNode != aTextNode) {
          nsIContent* child = aTextNode->GetNextNode(parent);
          while (child) {
            if (child->IsElement() &&
                (DoesNotParticipateInAutoDirection(child->AsElement()) ||
                 child->NodeInfo()->Equals(nsGkAtoms::bdi) ||
                 child->HasFixedDir())) {
              child = child->GetNextNonChildNode(parent);
              continue;
            }

            if (child == directionWasSetByTextNode) {
              // we found the node that set the element's direction after our
              // text node, so we need to reset the direction
              resetDirection = true;
              nsTextNodeDirectionalityMap::RemoveElementFromMap(
                directionWasSetByTextNode, parent
              );
              break;
            }

            child = child->GetNextNode(parent);
          }
        }
      }

      if (resetDirection) {
        parent->SetDirectionality(aDir, aNotify);
        nsTextNodeDirectionalityMap::AddEntryToMap(aTextNode, parent);
        SetDirectionalityOnDescendants(parent, aDir, aNotify);
      }

      // Since we found an element with dir=auto, we can stop walking the
      // parent chain: none of its ancestors will have their direction set by
      // any of its descendants.
      return;
    }
    parent = parent->GetElementParent();
  }
}

void
SetDirectionFromChangedTextNode(nsIContent* aTextNode, uint32_t aOffset,
                                const PRUnichar* aBuffer, uint32_t aLength,
                                bool aNotify)
{
  if (!NodeAffectsDirAutoAncestor(aTextNode)) {
    return;
  }

  uint32_t firstStrong;
  Directionality oldDir = GetDirectionFromText(aTextNode->GetText(),
                                               &firstStrong);
  if (aOffset > firstStrong) {
    return;
  }

  Directionality newDir = GetDirectionFromText(aBuffer, aLength);
  if (newDir == eDir_NotSet) {
    if (oldDir != eDir_NotSet && aTextNode->HasTextNodeDirectionalityMap()) {
      // This node used to have a strong directional character but no
      // longer does. ResetTextNodeDirection() will re-resolve the
      // directionality of any elements whose directionality was
      // determined by this node.
      nsTextNodeDirectionalityMap::ResetTextNodeDirection(aTextNode);
    }
  } else {
    // This node has a strong directional character. If it has a
    // TextNodeDirectionalityMap property, it already determines the
    // directionality of some element(s), so call UpdateTextNodeDirection to
    // reresolve their directionality. Otherwise call
    // SetAncestorDirectionIfAuto to find ancestor elements which should
    // have their directionality determined by this node.
    if (aTextNode->HasTextNodeDirectionalityMap()) {
      nsTextNodeDirectionalityMap::UpdateTextNodeDirection(aTextNode, newDir);
    } else {
      SetAncestorDirectionIfAuto(aTextNode, newDir, aNotify);
    }
  }
}

void
SetDirectionFromNewTextNode(nsTextNode* aTextNode)
{
  if (!NodeAffectsDirAutoAncestor(aTextNode)) {
    return;
  }

  Directionality dir = GetDirectionFromText(aTextNode->GetText());
  if (dir != eDir_NotSet) {
    SetAncestorDirectionIfAuto(aTextNode, dir);
  }
}

void
ResetDirectionSetByTextNode(nsTextNode* aTextNode)
{
  if (!NodeAffectsDirAutoAncestor(aTextNode)) {
    return;
  }

  Directionality dir = GetDirectionFromText(aTextNode->GetText());
  if (dir != eDir_NotSet && aTextNode->HasTextNodeDirectionalityMap()) {
    nsTextNodeDirectionalityMap::ResetTextNodeDirection(aTextNode);
  }
}

void
SetDirectionalityFromValue(Element* aElement, const nsAString& value,
                           bool aNotify)
{
  Directionality dir = GetDirectionFromText(PromiseFlatString(value).get(),
                                            value.Length());
  if (dir == eDir_NotSet) {
    dir = eDir_LTR;
  }

  aElement->SetDirectionality(dir, aNotify);
}

void
OnSetDirAttr(Element* aElement, const nsAttrValue* aNewValue,
             bool hadValidDir, bool aNotify)
{
  if (aElement->IsHTML() && aElement->NodeInfo()->Equals(nsGkAtoms::input)) {
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
  }

  if (aElement->HasDirAuto()) {
    WalkDescendantsSetDirAuto(aElement, aNotify);
  } else {
    SetDirectionalityOnDescendants(aElement,
                                   RecomputeDirectionality(aElement, aNotify),
                                   aNotify);
  }
}

} // end namespace mozilla

