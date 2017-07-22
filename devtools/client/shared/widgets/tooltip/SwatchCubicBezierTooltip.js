/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const defer = require("devtools/shared/defer");
const {Task} = require("devtools/shared/task");
const {CubicBezierWidget} = require("devtools/client/shared/widgets/CubicBezierWidget");
const SwatchBasedEditorTooltip = require("devtools/client/shared/widgets/tooltip/SwatchBasedEditorTooltip");

const Heritage = require("sdk/core/heritage");

const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * The swatch cubic-bezier tooltip class is a specific class meant to be used
 * along with rule-view's generated cubic-bezier swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * CubicBezierWidget.
 *
 * @param {Document} document
 *        The document to attach the SwatchCubicBezierTooltip. This is either the toolbox
 *        document if the tooltip is a popup tooltip or the panel's document if it is an
 *        inline editor.
 */
function SwatchCubicBezierTooltip(document) {
  SwatchBasedEditorTooltip.call(this, document);

  // Creating a cubic-bezier instance.
  // this.widget will always be a promise that resolves to the widget instance
  this.widget = this.setCubicBezierContent([0, 0, 1, 1]);
  this._onUpdate = this._onUpdate.bind(this);
}

SwatchCubicBezierTooltip.prototype = Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Fill the tooltip with a new instance of the cubic-bezier widget
   * initialized with the given value, and return a promise that resolves to
   * the instance of the widget
   */
  setCubicBezierContent: function (bezier) {
    let { doc } = this.tooltip;

    let container = doc.createElementNS(XHTML_NS, "div");
    container.className = "cubic-bezier-container";

    this.tooltip.setContent(container, { width: 510, height: 370 });

    let def = defer();

    // Wait for the tooltip to be shown before calling instanciating the widget
    // as it expect its DOM elements to be visible.
    this.tooltip.once("shown", () => {
      let widget = new CubicBezierWidget(container, bezier);
      def.resolve(widget);
    });

    return def.promise;
  },

  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set the cubic
   * bezier curve in the widget
   */
  show: Task.async(function* () {
    // Call the parent class' show function
    yield SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set the curve and listen to changes to preview them
    if (this.activeSwatch) {
      this.currentBezierValue = this.activeSwatch.nextSibling;
      this.widget.then(widget => {
        widget.off("updated", this._onUpdate);
        widget.cssCubicBezierValue = this.currentBezierValue.textContent;
        widget.on("updated", this._onUpdate);
        this.emit("ready");
      });
    }
  }),

  _onUpdate: function (event, bezier) {
    if (!this.activeSwatch) {
      return;
    }

    this.currentBezierValue.textContent = bezier + "";
    this.preview(bezier + "");
  },

  destroy: function () {
    SwatchBasedEditorTooltip.prototype.destroy.call(this);
    this.currentBezierValue = null;
    this.widget.then(widget => {
      widget.off("updated", this._onUpdate);
      widget.destroy();
    });
  }
});

module.exports = SwatchCubicBezierTooltip;
