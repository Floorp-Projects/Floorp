/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const SlottedNodeEditor = require("devtools/client/inspector/markup/views/slotted-node-editor");
const MarkupContainer = require("devtools/client/inspector/markup/views/markup-container");
const { extend } = require("devtools/shared/extend");

function SlottedNodeContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(this, markupView, node,
    "slottednodecontainer");

  this.editor = new SlottedNodeEditor(this, node);
  this.tagLine.appendChild(this.editor.elt);
  this.hasChildren = false;
}

SlottedNodeContainer.prototype = extend(MarkupContainer.prototype, {
  isDraggable: function() {
    return false;
  },

  isSlotted: function() {
    return true;
  }
});

module.exports = SlottedNodeContainer;
