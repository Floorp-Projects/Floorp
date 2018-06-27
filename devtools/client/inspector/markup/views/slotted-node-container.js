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
  _onMouseDown: function(event) {
    if (event.target.classList.contains("reveal-link")) {
      event.stopPropagation();
      event.preventDefault();
      return;
    }
    MarkupContainer.prototype._onMouseDown.call(this, event);
  },

  /**
   * Slotted node containers never display children and should not react to toggle.
   */
  _onToggle: function(event) {
    event.stopPropagation();
  },

  onContainerClick: async function(event) {
    if (!event.target.classList.contains("reveal-link")) {
      return;
    }

    const selection = this.markup.inspector.selection;
    if (selection.nodeFront != this.node || selection.isSlotted()) {
      const reason = "reveal-from-slot";
      this.markup.inspector.selection.setNodeFront(this.node, { reason });
    }
  },

  isDraggable: function() {
    return false;
  },

  isSlotted: function() {
    return true;
  }
});

module.exports = SlottedNodeContainer;
