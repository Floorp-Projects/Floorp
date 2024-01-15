/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DirectionalityUtils_h___
#define DirectionalityUtils_h___

#include "nscore.h"
#include "nsStringFwd.h"

class nsIContent;
class nsINode;
class nsAttrValue;
class nsTextNode;

namespace mozilla::dom {
class Element;
class HTMLSlotElement;
}  // namespace mozilla::dom

namespace mozilla {

enum class Directionality : uint8_t { Unset, Rtl, Ltr, Auto };

/**
 * Various methods for returning the directionality of a string using the
 * first-strong algorithm defined in http://unicode.org/reports/tr9/#P2
 *
 * @param[out] aFirstStrong the offset to the first character in the string with
 *             strong directionality, or UINT32_MAX if there is none (in which
 *             case the return value is Directionality::Unset).
 * @return the directionality of the string, or Unset if not available.
 */
Directionality GetDirectionFromText(const char16_t* aText,
                                    const uint32_t aLength,
                                    uint32_t* aFirstStrong = nullptr);

/**
 * Set the directionality of an element according to the algorithm defined at
 * https://html.spec.whatwg.org/#the-directionality, not including elements with
 * auto direction.
 *
 * @return the directionality that the element was set to
 */
Directionality RecomputeDirectionality(mozilla::dom::Element* aElement,
                                       bool aNotify = true);

/**
 * Conceptually https://html.spec.whatwg.org/#parent-directionality, but a bit
 * different in how we deal with shadow DOM.
 *
 * FIXME(bug 1857719): Update directionality to the latest version of the spec.
 */
Directionality GetParentDirectionality(const mozilla::dom::Element* aElement);

/**
 * Set the directionality of any descendants of a node that do not themselves
 * have a dir attribute.
 * For performance reasons we walk down the descendant tree in the rare case
 * of setting the dir attribute, rather than walking up the ancestor tree in
 * the much more common case of getting the element's directionality.
 */
void SetDirectionalityOnDescendants(mozilla::dom::Element* aElement,
                                    Directionality aDir, bool aNotify = true);

/**
 * Walk the descendants of a node in tree order and, for any text node
 * descendant that determines the directionality of some element and is not a
 * descendant of another descendant of the original node with dir=auto,
 * redetermine that element's directionality
 */
void WalkDescendantsResetAutoDirection(mozilla::dom::Element* aElement);

/**
 * In case a slot element was added or removed it may change the directionality
 * of ancestors or assigned nodes.
 *
 * aAllAssignedNodesChanged forces the computation of the state for all of the
 * assigned descendants.
 */
void SlotStateChanged(dom::HTMLSlotElement* aSlot,
                      bool aAllAssignedNodesChanged = true);

/**
 * When only a single node in a slot is reassigned we can save some work
 * compared to the above.
 */
void SlotAssignedNodeChanged(dom::HTMLSlotElement* aSlot,
                             nsIContent& aAssignedNode);

/**
 * After setting dir=auto on an element, walk its descendants in tree order.
 * If the node doesn't have the NODE_ANCESTOR_HAS_DIR_AUTO flag, set the
 * NODE_ANCESTOR_HAS_DIR_AUTO flag on all of its descendants.
 * Resolve the directionality of the element by the "downward propagation
 * algorithm" (defined in section 3 in the comments at the beginning of
 * DirectionalityUtils.cpp)
 */
void WalkDescendantsSetDirAuto(mozilla::dom::Element* aElement,
                               bool aNotify = true);

/**
 * After unsetting dir=auto on an element, walk its descendants in tree order,
 * skipping any that have dir=auto themselves, and unset the
 * NODE_ANCESTOR_HAS_DIR_AUTO flag
 */
void WalkDescendantsClearAncestorDirAuto(nsIContent* aContent);

/**
 * When the contents of a text node are about to change, retrieve the current
 * directionality of the text
 *
 * @return whether the text node affects the directionality of any element
 */
bool TextNodeWillChangeDirection(nsTextNode* aTextNode, Directionality* aOldDir,
                                 uint32_t aOffset);

/**
 * After the contents of a text node have changed, change the directionality
 * of any elements whose directionality is determined by that node
 */
void TextNodeChangedDirection(nsTextNode* aTextNode, Directionality aOldDir,
                              bool aNotify);

/**
 * When a text node is appended to an element, find any ancestors with dir=auto
 * whose directionality will be determined by the text node
 */
void SetDirectionFromNewTextNode(nsTextNode* aTextNode);

/**
 * When a text node is removed from a document, find any ancestors whose
 * directionality it determined and redetermine their directionality
 *
 * @param aTextNode the text node
 */
void ResetDirectionSetByTextNode(nsTextNode* aTextNode);

/**
 * Set the directionality of an element according to the directionality of the
 * text in aValue
 */
void SetDirectionalityFromValue(mozilla::dom::Element* aElement,
                                const nsAString& aValue, bool aNotify);

/**
 * Called when setting the dir attribute on an element, immediately after
 * AfterSetAttr. This is instead of using BeforeSetAttr or AfterSetAttr, because
 * in AfterSetAttr we don't know the old value, so we can't identify all cases
 * where we need to walk up or down the document tree and reset the direction;
 * and in BeforeSetAttr we can't do the walk because this element hasn't had the
 * value set yet so the results will be wrong.
 */
void OnSetDirAttr(mozilla::dom::Element* aElement, const nsAttrValue* aNewValue,
                  bool hadValidDir, bool hadDirAuto, bool aNotify);

/**
 * Called when binding a new element to the tree, to set the
 * NodeAncestorHasDirAuto flag and set the direction of the element and its
 * ancestors if necessary
 */
void SetDirOnBind(mozilla::dom::Element* aElement, nsIContent* aParent);

/**
 * Called when unbinding an element from the tree, to recompute the
 * directionality of the element if it doesn't have autodirection, and to
 * clean up any entries in nsTextDirectionalityMap that refer to it.
 */
void ResetDir(mozilla::dom::Element* aElement);
}  // end namespace mozilla

#endif /* DirectionalityUtils_h___ */
