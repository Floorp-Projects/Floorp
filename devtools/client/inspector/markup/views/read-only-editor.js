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
  this.buildMarkup();

  if (node.isPseudoElement) {
    this.tag.classList.add("theme-fg-color3");
    if (node.isMarkerPseudoElement) {
      this.tag.textContent = "::marker";
    } else if (node.isBeforePseudoElement) {
      this.tag.textContent = "::before";
    } else if (node.isAfterPseudoElement) {
      this.tag.textContent = "::after";
    }
  } else if (node.nodeType == nodeConstants.DOCUMENT_TYPE_NODE) {
    this.elt.classList.add("comment", "doctype");
    this.tag.textContent = node.doctypeString;
  } else if (node.isShadowRoot) {
    this.tag.textContent = `#shadow-root (${node.shadowRootMode})`;
  } else {
    this.tag.textContent = node.nodeName;
  }

  // Make the "tag" part of this editor focusable.
  this.tag.setAttribute("tabindex", "-1");
}

ReadOnlyEditor.prototype = {
  buildMarkup: function() {
    const doc = this.markup.doc;

    this.elt = doc.createElement("span");
    this.elt.classList.add("editor");

    this.tag = doc.createElement("span");
    this.tag.classList.add("tag");
    this.elt.appendChild(this.tag);
  },

  destroy: function() {
    // We might be already destroyed.
    if (!this.elt) {
      return;
    }

    this.elt.remove();
    this.elt = null;
    this.tag = null;
  },

  /**
   * Stub method for consistency with ElementEditor.
   */
  getInfoAtNode: function() {
    return null;
  },
};

module.exports = ReadOnlyEditor;
