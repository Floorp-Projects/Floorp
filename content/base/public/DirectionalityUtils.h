/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DirectionalityUtils_h___
#define DirectionalityUtils_h___

#include "nscore.h"

class nsIContent;
class nsIDocument;
class nsINode;
class nsAString;
class nsAttrValue;
class nsTextNode;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

namespace mozilla {

enum Directionality {
  eDir_NotSet,
  eDir_RTL,
  eDir_LTR,
  eDir_Auto
};

/**
 * Set the directionality of an element according to the algorithm defined at
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#the-directionality,
 * not including elements with auto direction.
 *
 * @return the directionality that the element was set to
 */
Directionality RecomputeDirectionality(mozilla::dom::Element* aElement,
                                       bool aNotify = true);

/**
 * Set the directionality of any descendants of a node that do not themselves
 * have a dir attribute.
 * For performance reasons we walk down the descendant tree in the rare case
 * of setting the dir attribute, rather than walking up the ancestor tree in
 * the much more common case of getting the element's directionality.
 */
void SetDirectionalityOnDescendants(mozilla::dom::Element* aElement,
                                    Directionality aDir,
                                    bool aNotify = true);

/**
 * Walk the descendants of a node in tree order and, for any text node
 * descendant that determines the directionality of some element and is not a
 * descendant of another descendant of the original node with dir=auto,
 * redetermine that element's directionality
  */
void WalkDescendantsResetAutoDirection(mozilla::dom::Element* aElement);

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
void WalkDescendantsClearAncestorDirAuto(mozilla::dom::Element* aElement);

/**
 * When the contents of a text node have changed, deal with any elements whose
 * directionality needs to change
 */
void SetDirectionFromChangedTextNode(nsIContent* aTextNode, uint32_t aOffset,
                                     const PRUnichar* aBuffer, uint32_t aLength,
                                     bool aNotify);

/**
 * When a text node is appended to an element, find any ancestors with dir=auto
 * whose directionality will be determined by the text node
 */
void SetDirectionFromNewTextNode(nsIContent* aTextNode);

/**
 * When a text node is removed from a document, find any ancestors whose
 * directionality it determined and redetermine their directionality
 */
void ResetDirectionSetByTextNode(nsTextNode* aTextNode);

/**
 * Set the directionality of an element according to the directionality of the
 * text in aValue
 */
void SetDirectionalityFromValue(mozilla::dom::Element* aElement,
                                const nsAString& aValue,
                                bool aNotify);

/**
 * Called when setting the dir attribute on an element, immediately after
 * AfterSetAttr. This is instead of using BeforeSetAttr or AfterSetAttr, because
 * in AfterSetAttr we don't know the old value, so we can't identify all cases
 * where we need to walk up or down the document tree and reset the direction;
 * and in BeforeSetAttr we can't do the walk because this element hasn't had the
 * value set yet so the results will be wrong.
 */
void OnSetDirAttr(mozilla::dom::Element* aElement,
                  const nsAttrValue* aNewValue,
                  bool hadValidDir,
                  bool hadDirAuto,
                  bool aNotify);

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
} // end namespace mozilla

#endif /* DirectionalityUtils_h___ */
