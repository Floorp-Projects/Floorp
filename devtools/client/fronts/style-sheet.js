/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { styleSheetSpec } = require("devtools/shared/specs/style-sheet");
const promise = require("promise");

loader.lazyRequireGetter(
  this,
  ["getIndentationFromPrefs", "getIndentationFromString"],
  "devtools/shared/indentation",
  true
);

/**
 * StyleSheetFront is the client-side counterpart to a StyleSheetActor.
 */
class StyleSheetFront extends FrontClassWithSpec(styleSheetSpec) {
  constructor(conn, targetFront, parentFront) {
    super(conn, targetFront, parentFront);

    this._onPropertyChange = this._onPropertyChange.bind(this);
    this.on("property-change", this._onPropertyChange);
  }

  destroy() {
    this.off("property-change", this._onPropertyChange);
    super.destroy();
  }

  _onPropertyChange(property, value) {
    this[property] = value;
  }

  form(form) {
    this.actorID = form.actor;
    this.href = form.href;
    this.nodeHref = form.nodeHref;
    this.disabled = form.disabled;
    this.title = form.title;
    this.system = form.system;
    this.styleSheetIndex = form.styleSheetIndex;
    this.ruleCount = form.ruleCount;
    this.sourceMapURL = form.sourceMapURL;
    this._sourceMapBaseURL = form.sourceMapBaseURL;
  }

  get isSystem() {
    return this.system;
  }

  get sourceMapBaseURL() {
    // Handle backward-compat for servers that don't return sourceMapBaseURL.
    if (this._sourceMapBaseURL === undefined) {
      return this.href || this.nodeHref;
    }

    return this._sourceMapBaseURL;
  }

  set sourceMapBaseURL(sourceMapBaseURL) {
    this._sourceMapBaseURL = sourceMapBaseURL;
  }

  /**
   * Get the indentation to use for edits to this style sheet.
   *
   * @return {Promise} A promise that will resolve to a string that
   * should be used to indent a block in this style sheet.
   */
  guessIndentation() {
    const prefIndent = getIndentationFromPrefs();
    if (prefIndent) {
      const { indentUnit, indentWithTabs } = prefIndent;
      return promise.resolve(indentWithTabs ? "\t" : " ".repeat(indentUnit));
    }

    return async function() {
      const longStr = await this.getText();
      const source = await longStr.string();

      const { indentUnit, indentWithTabs } = getIndentationFromString(source);

      return indentWithTabs ? "\t" : " ".repeat(indentUnit);
    }.bind(this)();
  }
}

exports.StyleSheetFront = StyleSheetFront;
registerFront(StyleSheetFront);
