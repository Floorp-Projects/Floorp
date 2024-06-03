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
function setVariableTooltip(tooltip, doc, text, registeredProperty) {
  // Create tooltip content
  const div = doc.createElementNS(XHTML_NS, "div");
  div.classList.add("devtools-monospace", "devtools-tooltip-css-variable");

  const valueEl = doc.createElementNS(XHTML_NS, "section");
  valueEl.classList.add("variable-value");
  valueEl.append(doc.createTextNode(text));
  div.appendChild(valueEl);

  // A registered property always have a non-falsy syntax
  if (registeredProperty?.syntax) {
    const dl = doc.createElementNS(XHTML_NS, "dl");
    dl.classList.add("registered-property");
    const addProperty = (label, value, lineBreak = true) => {
      const dt = doc.createElementNS(XHTML_NS, "dt");
      dt.append(doc.createTextNode(label));
      const dd = doc.createElementNS(XHTML_NS, "dd");
      dd.append(doc.createTextNode(value));
      dl.append(dt, dd);
      if (lineBreak) {
        dl.append(doc.createElementNS(XHTML_NS, "br"));
      }
    };

    const hasInitialValue = !!registeredProperty.initialValue;

    addProperty("syntax:", `"${registeredProperty.syntax}"`);
    addProperty("inherits:", registeredProperty.inherits, hasInitialValue);
    if (hasInitialValue) {
      addProperty("initial-value:", registeredProperty.initialValue, false);
    }

    div.appendChild(dl);
  }

  tooltip.panel.innerHTML = "";
  tooltip.panel.appendChild(div);
  tooltip.setContentSize({ width: "auto", height: "auto" });
}

module.exports.setVariableTooltip = setVariableTooltip;
