/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getIsJavascriptTracingEnabled,
  getJavascriptTracingLogMethod,
} from "../selectors/index";
import { PROMISE } from "./utils/middleware/promise";

/**
 * Toggle ON/OFF Javascript tracing for all targets.
 */
export function toggleTracing() {
  return async ({ dispatch, getState, client, panel }) => {
    // For now, the UI can only toggle all the targets all at once.
    const isTracingEnabled = getIsJavascriptTracingEnabled(getState());
    const logMethod = getJavascriptTracingLogMethod(getState());

    // Automatically open the split console when enabling tracing to the console
    if (!isTracingEnabled && logMethod == "console") {
      await panel.toolbox.openSplitConsole({ focusConsoleInput: false });
    }

    return dispatch({
      type: "TOGGLE_TRACING",
      [PROMISE]: client.toggleTracing(),
      enabled: !isTracingEnabled,
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
