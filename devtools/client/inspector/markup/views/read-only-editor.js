/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nodeConstants = require("devtools/shared/dom-node-constants");

/**
 * Creates an editor for non-editable nodes.
 */
function ReadOnlyEditor(container, node) {
  this.container = container;
  this.markup = this.container.markup;
  this.template = this.markup.template.bind(this.markup);
  this.elt = null;
  this.template("generic", this);

  if (node.isPseudoElement) {
    this.tag.classList.add("theme-fg-color5");
    this.tag.textContent = node.isBeforePseudoElement ? "::before" : "::after";
  } else if (node.nodeType == nodeConstants.DOCUMENT_TYPE_NODE) {
    this.elt.classList.add("comment", "doctype");
    this.tag.textContent = node.doctypeString;
  } else {
    this.tag.textContent = node.nodeName;
  }
}

ReadOnlyEditor.prototype = {
  destroy: function () {
    this.elt.remove();
  },

  /**
   * Stub method for consistency with ElementEditor.
   */
  getInfoAtNode: function () {
    return null;
  }
};

module.exports = ReadOnlyEditor;
