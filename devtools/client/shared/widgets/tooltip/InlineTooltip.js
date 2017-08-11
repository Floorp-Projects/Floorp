/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/old-event-emitter");

/**
 * The InlineTooltip can display widgets for the CSS Rules view in an
 * inline container.
 *
 * @param {Document} doc
 *        The toolbox document to attach the InlineTooltip container.
 */
function InlineTooltip(doc) {
  EventEmitter.decorate(this);

  this.doc = doc;

  this.panel = this.doc.createElement("div");

  this.topWindow = this._getTopWindow();
}

InlineTooltip.prototype = {
  /**
   * Show the tooltip. It might be wise to append some content first if you
   * don't want the tooltip to be empty.
   *
   * @param {Node} anchor
   *        Which node below which the tooltip should be shown.
   */
  show(anchor) {
    anchor.parentNode.insertBefore(this.panel, anchor.nextSibling);

    this.emit("shown");
  },

  /**
   * Hide the current tooltip.
   */
  hide() {
    if (!this.isVisible()) {
      return;
    }

    this.panel.parentNode.remove(this.panel);

    this.emit("hidden");
  },

  /**
   * Check if the tooltip is currently displayed.
   *
   * @return {Boolean} true if the tooltip is visible
   */
  isVisible() {
    return typeof this.panel.parentNode !== "undefined" && this.panel.parentNode !== null;
  },

  /**
   * Clears the HTML content of the tooltip panel
   */
  clear() {
    this.panel.innerHTML = "";
  },

  /**
   * Set the content of this tooltip. Will first clear the tooltip and then
   * append the new content element.
   *
   * @param {DOMNode} content
   *        A node that can be appended in the tooltip
   */
  setContent(content) {
    this.clear();

    this.panel.appendChild(content);
  },

  get content() {
    return this.panel.firstChild;
  },

  _getTopWindow: function () {
    return this.doc.defaultView;
  },

  destroy() {
    this.hide();
    this.doc = null;
    this.panel = null;
  },
};

module.exports = InlineTooltip;
