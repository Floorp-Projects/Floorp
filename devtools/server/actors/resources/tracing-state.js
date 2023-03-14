/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { TRACING_STATE },
} = require("resource://devtools/server/actors/resources/index.js");

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
    this.onAvailable = onAvailable;

    // Ensure dispatching the existing state, only if a tracer is enabled.
    const tracerActor = targetActor.getTargetScopedActor("tracer");
    if (!tracerActor || !tracerActor.isTracing()) {
      return;
    }

    this.onTracingToggled(true, tracerActor.getLogMethod());
  }

  /**
   * Stop watching for tracing state
   */
  destroy() {}

  // Emit a TRACING_STATE resource with:
  //   enabled = true|false
  //   logMethod = console|stdout
  // When Javascript tracing is enabled or disabled.
  onTracingToggled(enabled, logMethod) {
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
