/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
const {MdnDocsWidget} = require("devtools/client/shared/widgets/MdnDocsWidget");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const XHTML_NS = "http://www.w3.org/1999/xhtml";

const TOOLTIP_WIDTH = 418;
const TOOLTIP_HEIGHT = 308;

/**
 * Tooltip for displaying docs for CSS properties from MDN.
 *
 * @param {Document} toolboxDoc
 *        The toolbox document to attach the CSS docs tooltip.
 */
function CssDocsTooltip(toolboxDoc) {
  this.tooltip = new HTMLTooltip(toolboxDoc, {
    type: "arrow",
    consumeOutsideClicks: true,
    autofocus: true,
    useXulWrapper: true
  });
  this.widget = this.setMdnDocsContent();
  this._onVisitLink = this._onVisitLink.bind(this);
  this.widget.on("visitlink", this._onVisitLink);

  // Initialize keyboard shortcuts
  this.shortcuts = new KeyShortcuts({ window: this.tooltip.topWindow });
  this._onShortcut = this._onShortcut.bind(this);

  this.shortcuts.on("Escape", this._onShortcut);
}

CssDocsTooltip.prototype = {
  /**
   * Reports if the tooltip is currently shown
   *
   * @return {Boolean} True if the tooltip is displayed.
   */
  isVisible: function () {
    return this.tooltip.isVisible();
  },

  /**
   * Load CSS docs for the given property,
   * then display the tooltip.
   */
  show: function (anchor, propertyName) {
    this.tooltip.once("shown", () => {
      this.widget.loadCssDocs(propertyName);
    });
    this.tooltip.show(anchor);
  },

  hide: function () {
    this.tooltip.hide();
  },

  revert: function () {},

  _onShortcut: function (shortcut, event) {
    if (!this.tooltip.isVisible()) {
      return;
    }
    event.stopPropagation();
    event.preventDefault();
    this.hide();
  },

  _onVisitLink: function () {
    this.hide();
  },

  /**
   * Set the content of this tooltip to the MDN docs widget. This is called when the
   * tooltip is first constructed.
   * The caller can use the MdnDocsWidget to update the tooltip's  UI with new content
   * each time the tooltip is shown.
   *
   * @return {MdnDocsWidget} the created MdnDocsWidget instance.
   */
  setMdnDocsContent: function () {
    let container = this.tooltip.doc.createElementNS(XHTML_NS, "div");
    container.setAttribute("class", "mdn-container theme-body");
    this.tooltip.setContent(container, {width: TOOLTIP_WIDTH, height: TOOLTIP_HEIGHT});
    return new MdnDocsWidget(container);
  },

  destroy: function () {
    this.widget.off("visitlink", this._onVisitLink);
    this.widget.destroy();

    this.shortcuts.destroy();
    this.tooltip.destroy();
  }
};

module.exports = CssDocsTooltip;
