/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {getAutocompleteMaxWidth} = require("devtools/client/inspector/markup/utils");
const {editableField} = require("devtools/client/shared/inplace-editor");
const {getCssProperties} = require("devtools/shared/fronts/css-properties");
const {LocalizationHelper} = require("devtools/shared/l10n");

const INSPECTOR_L10N = new LocalizationHelper("devtools/locale/inspector.properties");

/**
 * Creates a simple text editor node, used for TEXT and COMMENT
 * nodes.
 *
 * @param  {MarkupContainer} container
 *         The container owning this editor.
 * @param  {DOMNode} node
 *         The node being edited.
 * @param  {String} templateId
 *         The template id to use to build the editor.
 */
function TextEditor(container, node, templateId) {
  this.container = container;
  this.markup = this.container.markup;
  this.node = node;
  this.template = this.markup.template.bind(templateId);
  this._selected = false;

  this.markup.template(templateId, this);

  editableField({
    element: this.value,
    stopOnReturn: true,
    trigger: "dblclick",
    multiline: true,
    maxWidth: () => getAutocompleteMaxWidth(this.value, this.container.elt),
    trimOutput: false,
    done: (val, commit) => {
      if (!commit) {
        return;
      }
      this.node.getNodeValue().then(longstr => {
        longstr.string().then(oldValue => {
          longstr.release().then(null, console.error);

          this.container.undo.do(() => {
            this.node.setNodeValue(val);
          }, () => {
            this.node.setNodeValue(oldValue);
          });
        });
      });
    },
    cssProperties: getCssProperties(this.markup.toolbox),
    contextMenu: this.markup.inspector.onTextBoxContextMenu
  });

  this.update();
}

TextEditor.prototype = {
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

  update: function () {
    let longstr = null;
    this.node.getNodeValue().then(ret => {
      longstr = ret;
      return longstr.string();
    }).then(str => {
      longstr.release().then(null, console.error);
      this.value.textContent = str;

      let isWhitespace = !/[^\s]/.exec(str);
      this.value.classList.toggle("whitespace", isWhitespace);

      let chars = str.replace(/\n/g, "⏎")
                     .replace(/\t/g, "⇥")
                     .replace(/ /g, "◦");
      this.value.setAttribute("title", isWhitespace
        ? INSPECTOR_L10N.getFormatStr("markupView.whitespaceOnly", chars)
        : "");
    }).then(null, console.error);
  },

  destroy: function () {},

  /**
   * Stub method for consistency with ElementEditor.
   */
  getInfoAtNode: function () {
    return null;
  }
};

module.exports = TextEditor;
