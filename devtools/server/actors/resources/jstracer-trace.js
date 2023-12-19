/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class JSTraceWatcher {
  /**
   * Start watching for traces for a given target actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    this.#onAvailable = onAvailable;
  }

  #onAvailable;

  /**
   * Stop watching for traces
   */
  destroy() {
    // The traces are being emitted by the TracerActor via `emitTraces` method,
    // we start and stop recording and emitting tracer from this actor.
    // Watching for JSTRACER_TRACE only allows receiving these trace events.
  }

  /**
   * Emit a JSTRACER_TRACE resource.
   *
   * This is being called by the Tracer Actor.
   */
  emitTraces(traces) {
    this.#onAvailable(traces);
  }
}

module.exports = JSTraceWatcher;
