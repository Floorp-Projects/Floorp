/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Task} = require("devtools/shared/task");
const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const SwatchBasedEditorTooltip = require("devtools/client/shared/widgets/tooltip/SwatchBasedEditorTooltip");

const Heritage = require("sdk/core/heritage");

const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * The swatch-based css filter tooltip class is a specific class meant to be
 * used along with rule-view's generated css filter swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * CSSFilterEditorWidget.
 *
 * @param {Document} document
 *        The document to attach the SwatchFilterTooltip. This is either the toolbox
 *        document if the tooltip is a popup tooltip or the panel's document if it is an
 *        inline editor.
 * @param {function} cssIsValid
 *        A function to check that css declaration's name and values are valid together.
 *        This can be obtained from "shared/fronts/css-properties.js".
 */
function SwatchFilterTooltip(document, cssIsValid) {
  SwatchBasedEditorTooltip.call(this, document);
  this._cssIsValid = cssIsValid;

  // Creating a filter editor instance.
  this.widget = this.setFilterContent("none");
  this._onUpdate = this._onUpdate.bind(this);
}

SwatchFilterTooltip.prototype = Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Fill the tooltip with a new instance of the CSSFilterEditorWidget
   * widget initialized with the given filter value, and return a promise
   * that resolves to the instance of the widget when ready.
   */
  setFilterContent: function (filter) {
    let { doc } = this.tooltip;

    let container = doc.createElementNS(XHTML_NS, "div");
    container.id = "filter-container";

    this.tooltip.setContent(container, { width: 510, height: 200 });

    return new CSSFilterEditorWidget(container, filter, this._cssIsValid);
  },

  show: Task.async(function* () {
    // Call the parent class' show function
    yield SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set the filter value and listen to changes to preview them
    if (this.activeSwatch) {
      this.currentFilterValue = this.activeSwatch.nextSibling;
      this.widget.off("updated", this._onUpdate);
      this.widget.on("updated", this._onUpdate);
      this.widget.setCssValue(this.currentFilterValue.textContent);
      this.widget.render();
      this.emit("ready");
    }
  }),

  _onUpdate: function (event, filters) {
    if (!this.activeSwatch) {
      return;
    }

    // Remove the old children and reparse the property value to
    // recompute them.
    while (this.currentFilterValue.firstChild) {
      this.currentFilterValue.firstChild.remove();
    }
    let node = this._parser.parseCssProperty("filter", filters, this._options);
    this.currentFilterValue.appendChild(node);

    this.preview();
  },

  destroy: function () {
    SwatchBasedEditorTooltip.prototype.destroy.call(this);
    this.currentFilterValue = null;
    this.widget.off("updated", this._onUpdate);
    this.widget.destroy();
  },

  /**
   * Like SwatchBasedEditorTooltip.addSwatch, but accepts a parser object
   * to use when previewing the updated property value.
   *
   * @param {node} swatchEl
   *        @see SwatchBasedEditorTooltip.addSwatch
   * @param {object} callbacks
   *        @see SwatchBasedEditorTooltip.addSwatch
   * @param {object} parser
   *        A parser object; @see OutputParser object
   * @param {object} options
   *        options to pass to the output parser, with
   *          the option |filterSwatch| set.
   */
  addSwatch: function (swatchEl, callbacks, parser, options) {
    SwatchBasedEditorTooltip.prototype.addSwatch.call(this, swatchEl,
                                                      callbacks);
    this._parser = parser;
    this._options = options;
  }
});

module.exports = SwatchFilterTooltip;
