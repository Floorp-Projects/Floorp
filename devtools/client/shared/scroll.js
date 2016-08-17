/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Scroll the document so that the element "elem" appears in the viewport.
 *
 * @param {DOMNode} elem
 *        The element that needs to appear in the viewport.
 * @param {Boolean} centered
 *        true if you want it centered, false if you want it to appear on the
 *        top of the viewport. It is true by default, and that is usually what
 *        you want.
 */
function scrollIntoViewIfNeeded(elem, centered = true) {
  let win = elem.ownerDocument.defaultView;
  let clientRect = elem.getBoundingClientRect();

  // The following are always from the {top, bottom}
  // of the viewport, to the {top, …} of the box.
  // Think of them as geometrical vectors, it helps.
  // The origin is at the top left.

  let topToBottom = clientRect.bottom;
  let bottomToTop = clientRect.top - win.innerHeight;
  // We allow one translation on the y axis.
  let yAllowed = true;

  // Whatever `centered` is, the behavior is the same if the box is
  // (even partially) visible.
  if ((topToBottom > 0 || !centered) && topToBottom <= elem.offsetHeight) {
    win.scrollBy(0, topToBottom - elem.offsetHeight);
    yAllowed = false;
  } else if ((bottomToTop < 0 || !centered) &&
             bottomToTop >= -elem.offsetHeight) {
    win.scrollBy(0, bottomToTop + elem.offsetHeight);
    yAllowed = false;
  }

  // If we want it centered, and the box is completely hidden,
  // then we center it explicitly.
  if (centered) {
    if (yAllowed && (topToBottom <= 0 || bottomToTop >= 0)) {
      win.scroll(win.scrollX,
                 win.scrollY + clientRect.top
                 - (win.innerHeight - elem.offsetHeight) / 2);
    }
  }
}
exports.scrollIntoViewIfNeeded = scrollIntoViewIfNeeded;
