/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { FrontClass } = require("devtools/shared/protocol");
const {
  customHighlighterSpec,
  highlighterSpec
} = require("devtools/shared/specs/highlighters");

const HighlighterFront = FrontClass(highlighterSpec, {
  // Update the object given a form representation off the wire.
  form: function (json) {
    this.actorID = json.actor;
    // FF42+ HighlighterActors starts exposing custom form, with traits object
    this.traits = json.traits || {};
  }
});

exports.HighlighterFront = HighlighterFront;

const CustomHighlighterFront = FrontClass(customHighlighterSpec, {});

exports.CustomHighlighterFront = CustomHighlighterFront;
