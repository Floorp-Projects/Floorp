/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Set the tooltip content of a provided HTMLTooltip instance to display a
 * variable preview matching the provided text.
 *
 * @param  {HTMLTooltip} tooltip
 *         The tooltip instance on which the text preview content should be set.
 * @param  {Document} doc
 *         A document element to create the HTML elements needed for the tooltip.
 * @param  {String} text
 *         Text to display in tooltip.
 */
function setVariableTooltip(tooltip, doc, text) {
  // Create tooltip content
  const div = doc.createElementNS(XHTML_NS, "div");
  div.classList.add("devtools-monospace", "devtools-tooltip-css-variable");
  div.textContent = text;

  tooltip.setContent(div);
}

module.exports.setVariableTooltip = setVariableTooltip;
