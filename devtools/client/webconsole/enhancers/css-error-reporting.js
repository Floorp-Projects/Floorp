/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  INITIALIZE,
  FILTER_TOGGLE,
} = require("devtools/client/webconsole/constants");

/**
 * This is responsible for ensuring that error reporting is enabled if the CSS
 * filter is toggled on.
 */
function ensureCSSErrorReportingEnabled(hud) {
  return next => (reducer, initialState, enhancer) => {
    function ensureErrorReportingEnhancer(state, action) {
      const proxy = hud ? hud.proxy : null;
      if (!proxy) {
        return reducer(state, action);
      }

      state = reducer(state, action);
      if (!state.filters.css) {
        return state;
      }

      const cssFilterToggled =
        action.type == FILTER_TOGGLE && action.filter == "css";
      if (cssFilterToggled || action.type == INITIALIZE) {
        proxy.target.activeTab.ensureCSSErrorReportingEnabled();
      }
      return state;
    }
    return next(ensureErrorReportingEnhancer, initialState, enhancer);
  };
}

module.exports = ensureCSSErrorReportingEnabled;
