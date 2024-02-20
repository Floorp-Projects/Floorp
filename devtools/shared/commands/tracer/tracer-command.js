/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class TracerCommand {
  constructor({ commands }) {
    this.#targetConfigurationCommand = commands.targetConfigurationCommand;
    this.#resourceCommand = commands.resourceCommand;
  }

  #resourceCommand;
  #targetConfigurationCommand;
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
   * Get the dictionary passed to the server codebase as a SessionData.
   * This contains all settings to fine tune the tracer actual behavior.
   *
   * @return {JSON}
   *         Configuration object.
   */
  #getTracingOptions() {
    return {
      logMethod: Services.prefs.getStringPref(
        "devtools.debugger.javascript-tracing-log-method",
        ""
      ),
      traceValues: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-values",
        false
      ),
      traceOnNextInteraction: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-on-next-interaction",
        false
      ),
      traceOnNextLoad: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-on-next-load",
        false
      ),
      traceFunctionReturn: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-function-return",
        false
      ),
    };
  }

  /**
   * Toggle JavaScript tracing for all targets.
   */
  async toggle() {
    this.#isTracing = !this.#isTracing;

    await this.#targetConfigurationCommand.updateConfiguration({
      tracerOptions: this.#isTracing ? this.#getTracingOptions() : undefined,
    });
  }
}

module.exports = TracerCommand;
