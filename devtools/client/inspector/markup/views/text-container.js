/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nodeConstants = require("resource://devtools/shared/dom-node-constants.js");
const TextEditor = require("resource://devtools/client/inspector/markup/views/text-editor.js");
const MarkupContainer = require("resource://devtools/client/inspector/markup/views/markup-container.js");
const { extend } = require("resource://devtools/shared/extend.js");

/**
 * An implementation of MarkupContainer for text node and comment nodes.
 * Allows basic text editing in a textarea.
 *
 * @param  {MarkupView} markupView
 *         The markup view that owns this container.
 * @param  {NodeFront} node
 *         The node to display.
 * @param  {Inspector} inspector
 *         The inspector tool container the markup-view
 */
function MarkupTextContainer(markupView, node) {
  MarkupContainer.prototype.initialize.call(
    this,
    markupView,
    node,
    "textcontainer"
  );

  if (node.nodeType == nodeConstants.TEXT_NODE) {
    this.editor = new TextEditor(this, node, "text");
  } else if (node.nodeType == nodeConstants.COMMENT_NODE) {
    this.editor = new TextEditor(this, node, "comment");
  } else {
    throw new Error("Invalid node for MarkupTextContainer");
  }

  this.tagLine.appendChild(this.editor.elt);
}

MarkupTextContainer.prototype = extend(MarkupContainer.prototype, {});

module.exports = MarkupTextContainer;
