/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Force focus to an element that isn't focusable.
 * Toolbar buttons aren't focusable because if they were, clicking them would
 * focus them, which is undesirable. Therefore, they're only made focusable
 * when a user is navigating with the keyboard. This function forces focus as
 * is done during toolbar keyboard navigation.
 */
function forceFocus(aElem) {
  aElem.setAttribute("tabindex", "-1");
  aElem.focus();
  aElem.removeAttribute("tabindex");
}
