/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const {
  HTMLTooltip,
} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

/**
 * Tooltip displayed for when a CSS property is selected/highlighted.
 */
class RulePreviewTooltip {
  constructor(doc) {
    this.show = this.show.bind(this);
    this.destroy = this.destroy.bind(this);

    // Initialize tooltip structure.
    this._tooltip = new HTMLTooltip(doc, {
      type: "arrow",
      consumeOutsideClicks: true,
      useXulWrapper: true,
    });

    this.container = doc.createElementNS(XHTML_NS, "div");
    this.container.className = "rule-preview-tooltip-container";

    this.message = doc.createElementNS(XHTML_NS, "span");
    this.message.className = "rule-preview-tooltip-message";

    this.footer = doc.createElementNS(XHTML_NS, "span");
    this.footer.className = "rule-preview-tooltip-source-rule-footer";
    this.footer.textContent = L10N.getStr(
      "rulePreviewTooltip.jumpToRuleShortcut"
    );

    this._tooltip.panel.innerHTML = "";
    this._tooltip.panel.appendChild(this.container);
    this._tooltip.setContentSize({ width: "auto", height: "auto" });
  }

  /**
   * Shows the tooltip on a given element. The tooltip will display
   * the source CSS rule.
   *
   * @param  {Element} element
   *         The target element to show the tooltip with.
   * @param  {String} ruleText
   *         The source CSS rule to show in the tooltip.
   */
  show(element, ruleText) {
    this.message.textContent = ruleText;
    this.container.appendChild(this.message);
    this.container.appendChild(this.footer);
    this._tooltip.startTogglingOnHover(element);
  }

  destroy() {
    this._tooltip.destroy();
    this.container = null;
    this.message = null;
    this.footer = null;
  }
}

module.exports = RulePreviewTooltip;
