/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XULMAP_TYPE(menuseparator, XULMenuSeparatorAccessible)
XULMAP_TYPE(statusbar, XULStatusBarAccessible)

XULMAP(
  image,
  [](nsIContent* aContent, Accessible* aContext) -> Accessible* {
    if (aContent->IsElement() &&
        aContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::onclick)) {
      return new XULToolbarButtonAccessible(aContent, aContext->Document());
    }

    // Don't include nameless images in accessible tree.
    if (!aContent->IsElement() ||
        !aContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext)) {
      return nullptr;
    }

    return new ImageAccessibleWrap(aContent, aContext->Document());
  }
)
