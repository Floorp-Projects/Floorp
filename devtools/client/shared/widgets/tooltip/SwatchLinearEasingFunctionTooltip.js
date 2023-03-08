/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  LinearEasingFunctionWidget,
} = require("devtools/client/shared/widgets/LinearEasingFunctionWidget");
const SwatchBasedEditorTooltip = require("devtools/client/shared/widgets/tooltip/SwatchBasedEditorTooltip");

const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * The swatch linear-easing-function tooltip class is a specific class meant to be used
 * along with rule-view's generated linear-easing-function swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * LinearEasingFunctionWidget.
 *
 * @param {Document} document
 *        The document to attach the SwatchLinearEasingFunctionTooltip. This is either the toolbox
 *        document if the tooltip is a popup tooltip or the panel's document if it is an
 *        inline editor.
 */

class SwatchLinearEasingFunctionTooltip extends SwatchBasedEditorTooltip {
  constructor(document) {
    super(document);

    this.onWidgetUpdated = this.onWidgetUpdated.bind(this);

    // Creating a linear-easing-function instance.
    // this.widget will always be a promise that resolves to the widget instance
    this.widget = this.createWidget();
  }

  /**
   * Fill the tooltip with a new instance of the linear-easing-function widget
   * initialized with the given value, and return a promise that resolves to
   * the instance of the widget
   */

  async createWidget() {
    const { doc } = this.tooltip;
    this.tooltip.panel.innerHTML = "";

    const container = doc.createElementNS(XHTML_NS, "div");
    container.className = "linear-easing-function-container";

    this.tooltip.panel.appendChild(container);
    this.tooltip.setContentSize({ width: 400, height: 400 });

    await this.tooltip.once("shown");
    return new LinearEasingFunctionWidget(container);
  }

  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set the linear function line
   * in the widget
   */
  async show() {
    // Call the parent class' show function
    await super.show();
    // Then set the line and listen to changes to preview them
    if (this.activeSwatch) {
      this.ruleViewCurrentLinearValueElement = this.activeSwatch.nextSibling;
      this.widget.then(widget => {
        widget.off("updated", this.onWidgetUpdated);
        widget.setCssLinearValue(this.activeSwatch.getAttribute("data-linear"));
        widget.on("updated", this.onWidgetUpdated);
        this.emit("ready");
      });
    }
  }

  onWidgetUpdated(newValue) {
    if (!this.activeSwatch) {
      return;
    }

    this.ruleViewCurrentLinearValueElement.textContent = newValue;
    this.activeSwatch.setAttribute("data-linear", newValue);
    this.preview(newValue);
  }

  destroy() {
    super.destroy();
    this.currentFunctionText = null;
    this.widget.then(widget => {
      widget.off("updated", this.onWidgetUpdated);
      widget.destroy();
    });
  }
}

module.exports = SwatchLinearEasingFunctionTooltip;
