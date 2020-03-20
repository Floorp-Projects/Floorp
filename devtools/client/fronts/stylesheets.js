/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  mediaRuleSpec,
  styleSheetSpec,
  styleSheetsSpec,
} = require("devtools/shared/specs/stylesheets");
const promise = require("promise");

loader.lazyRequireGetter(
  this,
  "getIndentationFromPrefs",
  "devtools/shared/indentation",
  true
);
loader.lazyRequireGetter(
  this,
  "getIndentationFromString",
  "devtools/shared/indentation",
  true
);

/**
 * Corresponding client-side front for a MediaRuleActor.
 */
class MediaRuleFront extends FrontClassWithSpec(mediaRuleSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this._onMatchesChange = this._onMatchesChange.bind(this);
    this.on("matches-change", this._onMatchesChange);
  }

  _onMatchesChange(matches) {
    this._form.matches = matches;
  }

  form(form) {
    this.actorID = form.actor;
    this._form = form;
  }

  get mediaText() {
    return this._form.mediaText;
  }
  get conditionText() {
    return this._form.conditionText;
  }
  get matches() {
    return this._form.matches;
  }
  get line() {
    return this._form.line || -1;
  }
  get column() {
    return this._form.column || -1;
  }
  get parentStyleSheet() {
    return this.conn.getFrontByID(this._form.parentStyleSheet);
  }
}

exports.MediaRuleFront = MediaRuleFront;
registerFront(MediaRuleFront);

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
    this._form[property] = value;
  }

  form(form) {
    this.actorID = form.actor;
    this._form = form;
  }

  get href() {
    return this._form.href;
  }
  get nodeHref() {
    return this._form.nodeHref;
  }
  get disabled() {
    return !!this._form.disabled;
  }
  get title() {
    return this._form.title;
  }
  get isSystem() {
    return this._form.system;
  }
  get styleSheetIndex() {
    return this._form.styleSheetIndex;
  }
  get ruleCount() {
    return this._form.ruleCount;
  }
  get sourceMapURL() {
    return this._form.sourceMapURL;
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

/**
 * The corresponding Front object for the StyleSheetsActor.
 */
class StyleSheetsFront extends FrontClassWithSpec(styleSheetsSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "styleSheetsActor";
  }
}

exports.StyleSheetsFront = StyleSheetsFront;
registerFront(StyleSheetsFront);
