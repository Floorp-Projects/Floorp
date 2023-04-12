/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { TRACING_STATE },
} = require("resource://devtools/server/actors/resources/index.js");

// Bug 1827382, as this module can be used from the worker thread,
// the following JSM may be loaded by the worker loader until
// we have proper support for ESM from workers.
const {
  addTracingListener,
  removeTracingListener,
} = require("resource://devtools/server/tracer/tracer.jsm");

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

  // Emit a TRACING_STATE resource with:
  //   enabled = true|false
  // When Javascript tracing is enabled or disabled.
  onTracingToggled(enabled) {
    const tracerActor = this.targetActor.getTargetScopedActor("tracer");
    const logMethod = tracerActor?.getLogMethod() | "stdout";
    this.onAvailable([
      {
        resourceType: TRACING_STATE,
        enabled,
        logMethod,
      },
    ]);
  }
}

module.exports = TracingStateWatcher;
