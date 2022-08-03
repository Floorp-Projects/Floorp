/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const protocol = require("devtools/shared/protocol");
const { mediaRuleSpec } = require("devtools/shared/specs/media-rule");
const InspectorUtils = require("InspectorUtils");

/**
 * A MediaRuleActor lives on the server and provides access to properties
 * of a DOM @media rule and emits events when it changes.
 */
var MediaRuleActor = protocol.ActorClassWithSpec(mediaRuleSpec, {
  get window() {
    return this.parentActor.window;
  },

  get document() {
    return this.window.document;
  },

  get matches() {
    return this.mql ? this.mql.matches : null;
  },

  initialize(mediaRule, parentActor) {
    protocol.Actor.prototype.initialize.call(this, parentActor.conn);

    this.rawRule = mediaRule;
    this.parentActor = parentActor;
    this.conn = this.parentActor.conn;

    this._matchesChange = this._matchesChange.bind(this);

    this.line = InspectorUtils.getRelativeRuleLine(mediaRule);
    this.column = InspectorUtils.getRuleColumn(mediaRule);

    try {
      this.mql = this.window.matchMedia(mediaRule.media.mediaText);
    } catch (e) {
      // Ignored
    }

    if (this.mql) {
      this.mql.addListener(this._matchesChange);
    }
  },

  destroy() {
    if (this.mql) {
      // The content page may already be destroyed and mql be the dead wrapper.
      if (!Cu.isDeadWrapper(this.mql)) {
        this.mql.removeListener(this._matchesChange);
      }
      this.mql = null;
    }

    protocol.Actor.prototype.destroy.call(this);
  },

  form() {
    const form = {
      actor: this.actorID, // actorID is set when this is added to a pool
      mediaText: this.rawRule.media.mediaText,
      conditionText: this.rawRule.conditionText,
      matches: this.matches,
      line: this.line,
      column: this.column,
      parentStyleSheet: this.parentActor.actorID,
    };

    return form;
  },

  _matchesChange() {
    this.emit("matches-change", this.matches);
  },
});
exports.MediaRuleActor = MediaRuleActor;
