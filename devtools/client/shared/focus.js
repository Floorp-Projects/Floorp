/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Simplied selector targetting elements that can receive the focus, full
 * version at http://stackoverflow.com/questions/1599660/which-html-elements-can-receive-focus
 */
const focusableSelector = [
  "a[href]:not([tabindex='-1'])",
  "button:not([disabled]):not([tabindex='-1'])",
  "iframe:not([tabindex='-1'])",
  "input:not([disabled]):not([tabindex='-1'])",
  "select:not([disabled]):not([tabindex='-1'])",
  "textarea:not([disabled]):not([tabindex='-1'])",
  "[tabindex]:not([tabindex='-1'])",
].join(", ");

/**
 * Wrap and move keyboard focus to first/last focusable element inside a container
 * element to prevent the focus from escaping the container.
 *
 * @param  {Array} elms
 *         focusable elements inside a container
 * @param  {DOMNode} current
 *         currently focused element inside containter
 * @param  {Boolean} back
 *         direction
 * @return {DOMNode}
 *         newly focused element
 */
function wrapMoveFocus(elms, current, back) {
  let next;

  if (elms.length === 0) {
    return false;
  }

  if (back) {
    if (elms.indexOf(current) === 0) {
      next = elms[elms.length - 1];
      next.focus();
    }
  } else if (elms.indexOf(current) === elms.length - 1) {
    next = elms[0];
    next.focus();
  }

  return next;
}

/**
 * Get a list of all elements that are focusable with a keyboard inside the parent element
 *
 * @param  {DOMNode} parentEl
 *         parent DOM element to be queried
 * @return {Array}
 *         array of focusable children elements inside the parent
 */
function getFocusableElements(parentEl) {
  return parentEl
    ? Array.from(parentEl.querySelectorAll(focusableSelector))
    : [];
}

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  module.exports.focusableSelector = focusableSelector;
  exports.wrapMoveFocus = wrapMoveFocus;
  exports.getFocusableElements = getFocusableElements;
});
