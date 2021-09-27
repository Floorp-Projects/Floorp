/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for a frame / docShell in the content process (where the actual
 * content lives).
 *
 * This actor extends WindowGlobalTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const {
  windowGlobalTargetPrototype,
} = require("devtools/server/actors/targets/window-global");

const { extend } = require("devtools/shared/extend");
const { frameTargetSpec } = require("devtools/shared/specs/targets/frame");
const Targets = require("devtools/server/actors/targets/index");
const TargetActorMixin = require("devtools/server/actors/targets/target-actor-mixin");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of WindowGlobalTargetActor.prototype
 *
 * Target actor for a frame / docShell in the content process.
 */
const frameTargetPrototype = extend({}, windowGlobalTargetPrototype);

Object.defineProperty(frameTargetPrototype, "title", {
  get: function() {
    return this.window.document.title;
  },
  enumerable: true,
  configurable: true,
});

exports.FrameTargetActor = TargetActorMixin(
  Targets.TYPES.FRAME,
  frameTargetSpec,
  frameTargetPrototype
);
