/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Front, FrontClassWithSpec } = require("devtools/shared/protocol");
const {
  getIndentationFromPrefs,
  getIndentationFromString
} = require("devtools/shared/indentation");
const {
  originalSourceSpec,
  mediaRuleSpec,
  styleSheetSpec,
  styleSheetsSpec
} = require("devtools/shared/specs/stylesheets");
const promise = require("promise");
const { Task } = require("resource://gre/modules/Task.jsm");
const events = require("sdk/event/core");

/**
 * The client-side counterpart for an OriginalSourceActor.
 */
const OriginalSourceFront = FrontClassWithSpec(originalSourceSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);

    this.isOriginalSource = true;
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
  },

  get href() {
    return this._form.url;
  },
  get url() {
    return this._form.url;
  }
});

exports.OriginalSourceFront = OriginalSourceFront;

/**
 * Corresponding client-side front for a MediaRuleActor.
 */
const MediaRuleFront = FrontClassWithSpec(mediaRuleSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);

    this._onMatchesChange = this._onMatchesChange.bind(this);
    events.on(this, "matches-change", this._onMatchesChange);
  },

  _onMatchesChange: function (matches) {
    this._form.matches = matches;
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
  },

  get mediaText() {
    return this._form.mediaText;
  },
  get conditionText() {
    return this._form.conditionText;
  },
  get matches() {
    return this._form.matches;
  },
  get line() {
    return this._form.line || -1;
  },
  get column() {
    return this._form.column || -1;
  },
  get parentStyleSheet() {
    return this.conn.getActor(this._form.parentStyleSheet);
  }
});

exports.MediaRuleFront = MediaRuleFront;

/**
 * StyleSheetFront is the client-side counterpart to a StyleSheetActor.
 */
const StyleSheetFront = FrontClassWithSpec(styleSheetSpec, {
  initialize: function (conn, form) {
    Front.prototype.initialize.call(this, conn, form);

    this._onPropertyChange = this._onPropertyChange.bind(this);
    events.on(this, "property-change", this._onPropertyChange);
  },

  destroy: function () {
    events.off(this, "property-change", this._onPropertyChange);
    Front.prototype.destroy.call(this);
  },

  _onPropertyChange: function (property, value) {
    this._form[property] = value;
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
  },

  get href() {
    return this._form.href;
  },
  get nodeHref() {
    return this._form.nodeHref;
  },
  get disabled() {
    return !!this._form.disabled;
  },
  get title() {
    return this._form.title;
  },
  get isSystem() {
    return this._form.system;
  },
  get styleSheetIndex() {
    return this._form.styleSheetIndex;
  },
  get ruleCount() {
    return this._form.ruleCount;
  },

  /**
   * Get the indentation to use for edits to this style sheet.
   *
   * @return {Promise} A promise that will resolve to a string that
   * should be used to indent a block in this style sheet.
   */
  guessIndentation: function () {
    let prefIndent = getIndentationFromPrefs();
    if (prefIndent) {
      let {indentUnit, indentWithTabs} = prefIndent;
      return promise.resolve(indentWithTabs ? "\t" : " ".repeat(indentUnit));
    }

    return Task.spawn(function* () {
      let longStr = yield this.getText();
      let source = yield longStr.string();

      let {indentUnit, indentWithTabs} = getIndentationFromString(source);

      return indentWithTabs ? "\t" : " ".repeat(indentUnit);
    }.bind(this));
  }
});

exports.StyleSheetFront = StyleSheetFront;

/**
 * The corresponding Front object for the StyleSheetsActor.
 */
const StyleSheetsFront = FrontClassWithSpec(styleSheetsSpec, {
  initialize: function (client, tabForm) {
    Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.styleSheetsActor;
    this.manage(this);
  }
});

exports.StyleSheetsFront = StyleSheetsFront;
