/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyGetter(this, "TARGET_TYPES", function () {
  return require("resource://devtools/shared/commands/target/target-command.js")
    .TYPES;
});

class TracerCommand {
  constructor({ commands }) {
    this.#targetCommand = commands.targetCommand;
    this.#resourceCommand = commands.resourceCommand;
  }

  #resourceCommand;
  #targetCommand;
  #isTracing = false;

  async initialize() {
    return this.#resourceCommand.watchResources(
      [this.#resourceCommand.TYPES.JSTRACER_STATE],
      { onAvailable: this.onResourcesAvailable }
    );
  }
  destroy() {
    this.#resourceCommand.unwatchResources(
      [this.#resourceCommand.TYPES.JSTRACER_STATE],
      { onAvailable: this.onResourcesAvailable }
    );
  }

  onResourcesAvailable = resources => {
    for (const resource of resources) {
      if (resource.resourceType != this.#resourceCommand.TYPES.JSTRACER_STATE) {
        continue;
      }
      this.#isTracing = resource.enabled;
    }
  };

  /**
   * Toggle JavaScript tracing for all targets.
   *
   * @param {String} logMethod (optional)
   *        Where to log the traces. Can be console or stdout.
   */
  async toggle(logMethod) {
    this.#isTracing = !this.#isTracing;

    // If no explicit log method is passed, default to the preference value.
    if (!logMethod && this.#isTracing) {
      logMethod = Services.prefs.getStringPref(
        "devtools.debugger.javascript-tracing-log-method",
        ""
      );
    }

    const traceValues = Services.prefs.getBoolPref(
      "devtools.debugger.javascript-tracing-values",
      false
    );

    const traceOnNextInteraction = Services.prefs.getBoolPref(
      "devtools.debugger.javascript-tracing-on-next-interaction",
      false
    );

    const targets = this.#targetCommand.getAllTargets(
      this.#targetCommand.ALL_TYPES
    );
    await Promise.all(
      targets.map(async targetFront => {
        const tracerFront = await targetFront.getFront("tracer");

        // Bug 1848136: For now the tracer doesn't work for worker targets.
        if (tracerFront.targetType == TARGET_TYPES.WORKER) {
          return null;
        }

        if (this.#isTracing) {
          return tracerFront.startTracing(logMethod, {
            traceValues,
            traceOnNextInteraction,
          });
        }
        return tracerFront.stopTracing();
      })
    );
  }
}

module.exports = TracerCommand;
