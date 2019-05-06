/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  /**
   * Scroll the document so that the element "elem" appears in the viewport.
   *
   * @param {DOMNode} elem
   *        The element that needs to appear in the viewport.
   * @param {Boolean} centered
   *        true if you want it centered, false if you want it to appear on the
   *        top of the viewport. It is true by default, and that is usually what
   *        you want.
   * @param {Boolean} smooth
   *        true if you want the scroll to happen smoothly, instead of instantly.
   *        It is false by default.
   */
  function scrollIntoViewIfNeeded(elem, centered = true, smooth = false) {
    const win = elem.ownerDocument.defaultView;
    const clientRect = elem.getBoundingClientRect();

    // The following are always from the {top, bottom}
    // of the viewport, to the {top, …} of the box.
    // Think of them as geometrical vectors, it helps.
    // The origin is at the top left.

    const topToBottom = clientRect.bottom;
    const bottomToTop = clientRect.top - win.innerHeight;
    // We allow one translation on the y axis.
    let yAllowed = true;

    // disable smooth scrolling when the user prefers reduced motion
    const reducedMotion = win.matchMedia("(prefers-reduced-motion)").matches;
    smooth = smooth && !reducedMotion;

    const options = { behavior: smooth ? "smooth" : "auto" };

    // Whatever `centered` is, the behavior is the same if the box is
    // (even partially) visible.
    if ((topToBottom > 0 || !centered) && topToBottom <= elem.offsetHeight) {
      win.scrollBy(Object.assign(
        {left: 0, top: topToBottom - elem.offsetHeight}, options));
      yAllowed = false;
    } else if ((bottomToTop < 0 || !centered) &&
              bottomToTop >= -elem.offsetHeight) {
      win.scrollBy(Object.assign(
        {left: 0, top: bottomToTop + elem.offsetHeight}, options));

      yAllowed = false;
    }

    // If we want it centered, and the box is completely hidden,
    // then we center it explicitly.
    if (centered) {
      if (yAllowed && (topToBottom <= 0 || bottomToTop >= 0)) {
        const x = win.scrollX;
        const y = win.scrollY + clientRect.top -
          (win.innerHeight - elem.offsetHeight) / 2;
        win.scroll(Object.assign({left: x, top: y}, options));
      }
    }
  }

  function closestScrolledParent(node) {
    if (node == null) {
      return null;
    }

    if (node.scrollHeight > node.clientHeight) {
      return node;
    }

    return closestScrolledParent(node.parentNode);
  }

  /**
   * Scrolls the element into view if it is not visible.
   *
   * @param {DOMNode|undefined} element
   *        The item to be scrolled to.
   *
   * @param {Object|undefined} options
   *        An options object which can contain:
   *          - container: possible scrollable container. If it is not scrollable, we will
   *                       look it up.
   *          - alignTo:   "top" or "bottom" to indicate if we should scroll the element
   *                       to the top or the bottom of the scrollable container when the
   *                       element is off canvas.
   *          - center:    Indicate if we should scroll the element to the middle of the
   *                       scrollable container when the element is off canvas.
   */
  function scrollIntoView(element, options = {}) {
    if (!element) {
      return;
    }

    const { alignTo, center, container } = options;

    const { top, bottom } = element.getBoundingClientRect();
    const scrolledParent = closestScrolledParent(container || element.parentNode);
    const scrolledParentRect = scrolledParent ? scrolledParent.getBoundingClientRect() :
                                                null;
    const isVisible = !scrolledParent ||
      (top >= scrolledParentRect.top && bottom <= scrolledParentRect.bottom);

    if (isVisible) {
      return;
    }

    if (center) {
      element.scrollIntoView({ block: "center" });
      return;
    }

    const scrollToTop = alignTo ?
      alignTo === "top" : !scrolledParentRect || top < scrolledParentRect.top;
    element.scrollIntoView(scrollToTop);
  }

  // Exports from this module
  module.exports.scrollIntoViewIfNeeded = scrollIntoViewIfNeeded;
  module.exports.scrollIntoView = scrollIntoView;
});
