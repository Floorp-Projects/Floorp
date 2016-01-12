/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClass } = require("devtools/server/protocol");
const { ThreadActor } = require("devtools/server/actors/script");

/**
 * ChromeDebuggerActor is a thin wrapper over ThreadActor, slightly changing
 * some of its behavior.
 */
let ChromeDebuggerActor = ActorClass({
  typeName: "chromeDebugger",
  extends: ThreadActor,

  /**
   * Create the actor.
   *
   * @param parent object
   *        This actor's parent actor. See ThreadActor for a list of expected
   *        properties.
   */
  initialize: function(parent) {
    ThreadActor.call(this, parent);
  }
});

exports.ChromeDebuggerActor = ChromeDebuggerActor;
