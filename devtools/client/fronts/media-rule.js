/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { mediaRuleSpec } = require("devtools/shared/specs/media-rule");

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
