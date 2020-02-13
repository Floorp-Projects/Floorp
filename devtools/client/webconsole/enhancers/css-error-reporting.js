/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  INITIALIZE,
  FILTER_TOGGLE,
  TARGET_AVAILABLE,
} = require("devtools/client/webconsole/constants");

/**
 * This is responsible for ensuring that error reporting is enabled if the CSS
 * filter is toggled on.
 */
function ensureCSSErrorReportingEnabled(webConsoleUI) {
  return next => (reducer, initialState, enhancer) => {
    function ensureErrorReportingEnhancer(state, action) {
      const proxies = webConsoleUI ? webConsoleUI.getAllProxies() : null;
      if (!proxies) {
        return reducer(state, action);
      }

      state = reducer(state, action);
      if (!state.filters.css) {
        return state;
      }

      const cssFilterToggled =
        action.type == FILTER_TOGGLE && action.filter == "css";
      if (
        cssFilterToggled ||
        action.type == INITIALIZE ||
        action.type == TARGET_AVAILABLE
      ) {
        for (const proxy of proxies) {
          if (proxy.target && proxy.target.ensureCSSErrorReportingEnabled) {
            proxy.target.ensureCSSErrorReportingEnabled();
          }
        }
      }

      return state;
    }
    return next(ensureErrorReportingEnhancer, initialState, enhancer);
  };
}

module.exports = ensureCSSErrorReportingEnabled;
