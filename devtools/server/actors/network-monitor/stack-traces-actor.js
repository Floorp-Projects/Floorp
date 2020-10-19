/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { stackTracesSpec } = require("devtools/shared/specs/stacktraces");

loader.lazyRequireGetter(
  this,
  "WebConsoleUtils",
  "devtools/server/actors/webconsole/utils",
  true
);

const {
  TYPES: { NETWORK_EVENT_STACKTRACE },
  getResourceWatcher,
} = require("devtools/server/actors/resources/index");

/**
 * Manages the stacktraces of the network
 *
 * @constructor
 *
 */
const StackTracesActor = ActorClassWithSpec(stackTracesSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
  },

  destroy(conn) {
    Actor.prototype.destroy.call(this, conn);
  },

  /**
   * The "getStackTrace" packet type handler.
   *
   * @return object
   *         The response packet - stack trace.
   */
  getStackTrace(resourceId) {
    const networkEventStackTraceWatcher = getResourceWatcher(
      this.targetActor,
      NETWORK_EVENT_STACKTRACE
    );
    if (!networkEventStackTraceWatcher) {
      throw new Error("Not listening for network event stacktraces");
    }
    const stacktrace = networkEventStackTraceWatcher.getStackTrace(resourceId);
    return {
      stacktrace: WebConsoleUtils.removeFramesAboveDebuggerEval(stacktrace),
    };
  },
});

exports.StackTracesActor = StackTracesActor;
