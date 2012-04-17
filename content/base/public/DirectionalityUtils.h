/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DirectionalityUtils_h___
#define DirectionalityUtils_h___

class nsIContent;
class nsIDocument;
class nsINode;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

namespace mozilla {

namespace directionality {

enum Directionality {
  eDir_NotSet = 0,
  eDir_RTL    = 1,
  eDir_LTR    = 2
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

} // end namespace directionality

} // end namespace mozilla

#endif /* DirectionalityUtils_h___ */
