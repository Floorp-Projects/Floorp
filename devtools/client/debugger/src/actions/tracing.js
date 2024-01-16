/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getIsThreadCurrentlyTracing, getAllThreads } from "../selectors/index";
import { PROMISE } from "./utils/middleware/promise";

/**
 * Toggle ON/OFF Javascript tracing for all targets,
 * using the specified log method.
 *
 * @param {string} logMethod
 *        Can be "stdout" or "console". See TracerActor.
 */
export function toggleTracing(logMethod) {
  return async ({ dispatch, getState, client, panel }) => {
    // Check if any of the thread is currently tracing.
    // For now, the UI can only toggle all the targets all at once.
    const threads = getAllThreads(getState());
    const isTracingEnabled = threads.some(thread =>
      getIsThreadCurrentlyTracing(getState(), thread.actor)
    );

    // Automatically open the split console when enabling tracing to the console
    if (!isTracingEnabled && logMethod == "console") {
      await panel.toolbox.openSplitConsole({ focusConsoleInput: false });
    }

    return dispatch({
      type: "TOGGLE_TRACING",
      [PROMISE]: client.toggleTracing(logMethod),
    });
  };
}

/**
 * Called when tracing is toggled ON/OFF on a particular thread.
 */
export function tracingToggled(thread, enabled) {
  return ({ dispatch }) => {
    dispatch({
      type: "TRACING_TOGGLED",
      thread,
      enabled,
    });
  };
}
