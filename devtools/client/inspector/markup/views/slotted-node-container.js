/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const SlottedNodeEditor = require("devtools/client/inspector/markup/views/slotted-node-editor");
const MarkupContainer = require("devtools/client/inspector/markup/views/markup-container");
const { extend } = require("devtools/shared/extend");

function SlottedNodeContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(
    this,
    markupView,
    node,
    "slottednodecontainer"
  );

  this.editor = new SlottedNodeEditor(this, node);
  this.tagLine.appendChild(this.editor.elt);
  this.hasChildren = false;
}

SlottedNodeContainer.prototype = extend(MarkupContainer.prototype, {
  _onMouseDown(event) {
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
  _onToggle(event) {
    event.stopPropagation();
  },

  _revealFromSlot() {
    const reason = "reveal-from-slot";
    this.markup.inspector.selection.setNodeFront(this.node, { reason });
    this.markup.telemetry.scalarSet(
      "devtools.shadowdom.reveal_link_clicked",
      true
    );
  },

  _onKeyDown(event) {
    MarkupContainer.prototype._onKeyDown.call(this, event);

    const isActionKey = event.code == "Enter" || event.code == "Space";
    if (event.target.classList.contains("reveal-link") && isActionKey) {
      this._revealFromSlot();
    }
  },

  async onContainerClick(event) {
    if (!event.target.classList.contains("reveal-link")) {
      return;
    }

    this._revealFromSlot();
  },

  isDraggable() {
    return false;
  },

  isSlotted() {
    return true;
  },
});

module.exports = SlottedNodeContainer;
