/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for a frame / docShell in the content process (where the actual
 * content lives).
 *
 * This actor extends BrowsingContextTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

var {
  BrowsingContextTargetActor,
  browsingContextTargetPrototype,
} = require("devtools/server/actors/targets/browsing-context");

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { frameTargetSpec } = require("devtools/shared/specs/targets/frame");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using `extend` to
 * maintain the properties of BrowsingContextTargetActor.prototype
 */
const frameTargetPrototype = extend({}, browsingContextTargetPrototype);

/**
 * Target actor for a frame / docShell in the content process.
 *
 * @param connection DevToolsServerConnection
 *        The conection to the client.
 * @param docShell nsIDocShell
 *        The |docShell| for the debugged frame.
 * @param options Object
 *        See BrowsingContextTargetActor.initialize doc.
 */
frameTargetPrototype.initialize = function(connection, docShell, options) {
  BrowsingContextTargetActor.prototype.initialize.call(
    this,
    connection,
    options
  );

  this.traits.reconfigure = false;

  Object.defineProperty(this, "docShell", {
    value: docShell,
    configurable: true,
  });
};

Object.defineProperty(frameTargetPrototype, "title", {
  get: function() {
    return this.window.document.title;
  },
  enumerable: true,
  configurable: true,
});

exports.FrameTargetActor = ActorClassWithSpec(
  frameTargetSpec,
  frameTargetPrototype
);
