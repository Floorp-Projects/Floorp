/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Get the text of the element parameter, as provided by the Selection API. We need to
 * rely on the Selection API as it mimics exactly what the user would have if they do a
 * selection using the mouse and copy it. `HTMLElement.textContent` and
 * `HTMLElement.innerText` follow a different codepath than user selection + copy.
 * They both have issues when dealing with whitespaces, and therefore can't be used to
 * have a reliable result.
 *
 * As the Selection API is exposed through the Window object, if we don't have a window
 * we fallback to `HTMLElement.textContent`.
 *
 * @param {HTMLElement} el: The element we want the text of.
 * @returns {String|null} The text of the element, or null if el is falsy.
 */
function getElementText(el) {
  if (!el) {
    return null;
  }
  // If we can, we use the Selection API to match what the user would get if they
  // manually select and copy the message.
  const doc = el.ownerDocument;
  const win = doc && doc.defaultView;

  if (!win) {
    return el.textContent;
  }

  // We store the current selected range and unselect everything.
  const selection = win.getSelection();
  const currentSelectedRange =
    !selection.isCollapsed && selection.getRangeAt(0);
  selection.removeAllRanges();

  // Then creates a range from `el`, and get the text content.
  const range = doc.createRange();
  range.selectNode(el);
  selection.addRange(range);
  const text = selection.toString();

  // Finally we revert the selection to what it was.
  selection.removeRange(range);
  if (currentSelectedRange) {
    selection.addRange(currentSelectedRange);
  }

  return text;
}

module.exports = {
  getElementText,
};
