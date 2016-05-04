/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Front, FrontClassWithSpec } = require("devtools/server/protocol.js");
const {
  originalSourceSpec,
  mediaRuleSpec
} = require("devtools/shared/specs/stylesheets.js");
const events = require("sdk/event/core.js");

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
