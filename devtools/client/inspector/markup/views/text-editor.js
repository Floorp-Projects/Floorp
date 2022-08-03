/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");

const TextNode = createFactory(
  require("devtools/client/inspector/markup/components/TextNode")
);

loader.lazyRequireGetter(
  this,
  "getAutocompleteMaxWidth",
  "devtools/client/inspector/markup/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "getLongString",
  "devtools/client/inspector/shared/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "InplaceEditor",
  "devtools/client/shared/inplace-editor",
  true
);

/**
 * Creates a simple text editor node, used for TEXT and COMMENT
 * nodes.
 *
 * @param  {MarkupContainer} container
 *         The container owning this editor.
 * @param  {DOMNode} node
 *         The node being edited.
 * @param  {String} type
 *         The type of editor to build. This can be either 'text' or 'comment'.
 */
function TextEditor(container, node, type) {
  this.container = container;
  this.markup = this.container.markup;
  this.node = node;
  this._selected = false;

  this.showTextEditor = this.showTextEditor.bind(this);

  this.buildMarkup(type);
}

TextEditor.prototype = {
  buildMarkup(type) {
    const doc = this.markup.doc;

    this.elt = doc.createElement("span");
    this.elt.classList.add("editor", type);

    getLongString(this.node.getNodeValue()).then(value => {
      this.textNode = this.ReactDOM.render(
        TextNode({
          showTextEditor: this.showTextEditor,
          type,
          value,
        }),
        this.elt
      );
    });
  },

  get ReactDOM() {
    // Reuse the toolbox's ReactDOM to avoid loading react-dom.js again in the
    // Inspector's BrowserLoader.
    return this.container.markup.inspector.ReactDOM;
  },

  get selected() {
    return this._selected;
  },

  set selected(value) {
    if (value === this._selected) {
      return;
    }
    this._selected = value;
    this.update();
  },

  showTextEditor(element) {
    new InplaceEditor({
      cssProperties: this.markup.inspector.cssProperties,
      done: (val, commit) => {
        if (!commit) {
          return;
        }
        getLongString(this.node.getNodeValue()).then(oldValue => {
          this.container.undo.do(
            () => {
              this.node.setNodeValue(val);
            },
            () => {
              this.node.setNodeValue(oldValue);
            }
          );
        });
      },
      element,
      maxWidth: () => getAutocompleteMaxWidth(element, this.container.elt),
      multiline: true,
      stopOnReturn: true,
      trimOutput: false,
    });
  },

  async update() {
    try {
      const value = await getLongString(this.node.getNodeValue());

      if (this.textNode.state.value !== value) {
        this.textNode.setState({ value });
      }
    } catch (e) {
      console.error(e);
    }
  },

  destroy() {
    this.ReactDOM.unmountComponentAtNode(this.elt);
  },

  /**
   * Stub method for consistency with ElementEditor.
   */
  getInfoAtNode() {
    return null;
  },
};

module.exports = TextEditor;
