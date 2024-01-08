/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { JSTRACER_STATE },
} = require("resource://devtools/server/actors/resources/index.js");

// Bug 1827382, as this module can be used from the worker thread,
// the following JSM may be loaded by the worker loader until
// we have proper support for ESM from workers.
const {
  addTracingListener,
  removeTracingListener,
} = require("resource://devtools/server/tracer/tracer.jsm");

const { LOG_METHODS } = require("resource://devtools/server/actors/tracer.js");

class TracingStateWatcher {
  /**
   * Start watching for tracing state changes for a given target actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    this.targetActor = targetActor;
    this.onAvailable = onAvailable;

    this.tracingListener = {
      onTracingToggled: this.onTracingToggled.bind(this),
    };
    addTracingListener(this.tracingListener);
  }

  /**
   * Stop watching for tracing state
   */
  destroy() {
    removeTracingListener(this.tracingListener);
  }

  /**
   * Be notified by the underlying JavaScriptTracer class
   * in case it stops by itself, instead of being stopped when the Actor's stopTracing
   * method is called by the user.
   *
   * @param {Boolean} enabled
   *        True if the tracer starts tracing, false it it stops.
   * @param {String} reason
   *        Optional string to justify why the tracer stopped.
   */
  onTracingToggled(enabled, reason) {
    const tracerActor = this.targetActor.getTargetScopedActor("tracer");
    const logMethod = tracerActor?.getLogMethod();
    this.onAvailable([
      {
        resourceType: JSTRACER_STATE,
        enabled,
        logMethod,
        profile:
          logMethod == LOG_METHODS.PROFILER && !enabled
            ? tracerActor.getProfile()
            : undefined,
        timeStamp: ChromeUtils.dateNow(),
        reason,
      },
    ]);
  }
}

module.exports = TracingStateWatcher;
