/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const SELECTOR_WARNINGS = {
  UnconstrainedHas: {
    l10nId: "css-selector-warning-unconstrained-has",
    // There could be a specific section on the MDN :has page for this: https://github.com/mdn/mdn/issues/469
    learnMoreUrl: null,
  },
};

class CssSelectorWarningsTooltipHelper {
  /**
   * Fill the tooltip with selector warnings.
   */
  async setContent(data, tooltip) {
    const fragment = this.#getTemplate(data, tooltip);
    tooltip.panel.innerHTML = "";
    tooltip.panel.append(fragment);

    // Size the content.
    tooltip.setContentSize({ width: 267, height: Infinity });
  }

  /**
   * Get the template of the tooltip.
   *
   * @param {Array<String>} data: Array of selector warning kind returned by
   *        CSSRule#getSelectorWarnings
   * @param {HTMLTooltip} tooltip
   *        The tooltip we are targetting.
   */
  #getTemplate(data, tooltip) {
    const { doc } = tooltip;

    const templateNode = doc.createElementNS(XHTML_NS, "template");

    const tooltipContainer = doc.createElementNS(XHTML_NS, "ul");
    tooltipContainer.classList.add("devtools-tooltip-selector-warnings");
    templateNode.content.appendChild(tooltipContainer);

    for (const selectorWarningKind of data) {
      if (!SELECTOR_WARNINGS[selectorWarningKind]) {
        console.error("Unknown selector warning kind", data);
        continue;
      }

      const { l10nId } = SELECTOR_WARNINGS[selectorWarningKind];

      const li = doc.createElementNS(XHTML_NS, "li");
      li.setAttribute("data-l10n-id", l10nId);
      tooltipContainer.append(li);
    }

    return doc.importNode(templateNode.content, true);
  }
}

module.exports = CssSelectorWarningsTooltipHelper;
