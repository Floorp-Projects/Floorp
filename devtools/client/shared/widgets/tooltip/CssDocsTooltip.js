/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
const {MdnDocsWidget} = require("devtools/client/shared/widgets/MdnDocsWidget");
const XHTML_NS = "http://www.w3.org/1999/xhtml";

loader.lazyRequireGetter(this, "KeyShortcuts",
  "devtools/client/shared/key-shortcuts", true);

const TOOLTIP_WIDTH = 418;
const TOOLTIP_HEIGHT = 308;

/**
 * Tooltip for displaying docs for CSS properties from MDN.
 *
 * @param {Toolbox} toolbox
 *        Toolbox used to create the tooltip.
 */
function CssDocsTooltip(toolbox) {
  this.tooltip = new HTMLTooltip(toolbox, {
    type: "arrow",
    consumeOutsideClicks: true,
    autofocus: true,
    useXulWrapper: true,
    stylesheet: "chrome://devtools/content/shared/widgets/mdn-docs.css",
  });
  this.widget = this.setMdnDocsContent();

  // Initialize keyboard shortcuts
  this.shortcuts = new KeyShortcuts({ window: toolbox.doc.defaultView });
  this._onShortcut = this._onShortcut.bind(this);

  this.shortcuts.on("Escape", this._onShortcut);
  this.shortcuts.on("Return", this._onShortcut);
}

module.exports.CssDocsTooltip = CssDocsTooltip;

CssDocsTooltip.prototype = {
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

  _onShortcut: function (shortcut, event) {
    if (!this.tooltip.isVisible()) {
      return;
    }

    event.stopPropagation();
    if (shortcut === "Return") {
      // If user is pressing return, do not prevent default and delay hiding the tooltip
      // in case the focus is on the "Visit MDN page" link.
      this.tooltip.doc.defaultView.setTimeout(this.hide.bind(this), 0);
    } else {
      // For any other key, preventDefault() and hide straight away.
      event.preventDefault();
      this.hide();
    }
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
    this.shortcuts.destroy();
    this.tooltip.destroy();
  }
};
