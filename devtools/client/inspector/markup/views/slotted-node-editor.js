/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {LocalizationHelper} = require("devtools/shared/l10n");
const INSPECTOR_L10N =
      new LocalizationHelper("devtools/client/locales/inspector.properties");

function SlottedNodeEditor(container, node) {
  this.container = container;
  this.markup = this.container.markup;
  this.buildMarkup();
  this.tag.textContent = "→ <" + node.nodeName.toLowerCase() + ">";

  // Make the "tag" part of this editor focusable.
  this.tag.setAttribute("tabindex", "-1");
}

SlottedNodeEditor.prototype = {
  buildMarkup: function() {
    const doc = this.markup.doc;

    this.elt = doc.createElement("span");
    this.elt.classList.add("editor");

    this.tag = doc.createElement("span");
    this.tag.classList.add("tag");
    this.elt.appendChild(this.tag);

    this.revealLink = doc.createElement("span");
    this.revealLink.classList.add("reveal-link");
    this.revealLink.textContent = INSPECTOR_L10N.getStr("markupView.revealLink.label");
    this.elt.appendChild(this.revealLink);
  },

  destroy: function() {
    // We might be already destroyed.
    if (!this.elt) {
      return;
    }

    this.elt.remove();
    this.elt = null;
    this.tag = null;
    this.revealLink = null;
  },

  /**
   * Stub method for consistency with ElementEditor.
   */
  getInfoAtNode: function() {
    return null;
  }
};

module.exports = SlottedNodeEditor;
