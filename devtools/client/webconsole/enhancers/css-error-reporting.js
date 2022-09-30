/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  INITIALIZE,
  FILTER_TOGGLE,
  FILTERS,
} = require("resource://devtools/client/webconsole/constants.js");

/**
 * This is responsible for ensuring that error reporting is enabled if the CSS
 * filter is toggled on.
 */
function ensureCSSErrorReportingEnabled(webConsoleUI) {
  let watchingCSSMessages = false;
  return next => (reducer, initialState, enhancer) => {
    function ensureErrorReportingEnhancer(state, action) {
      state = reducer(state, action);

      // If we're already watching CSS messages, or if the CSS filter is disabled,
      // we don't do anything.
      if (!webConsoleUI || watchingCSSMessages || !state.filters.css) {
        return state;
      }

      const cssFilterToggled =
        action.type == FILTER_TOGGLE && action.filter == FILTERS.CSS;

      if (cssFilterToggled || action.type == INITIALIZE) {
        watchingCSSMessages = true;
        webConsoleUI.watchCssMessages();
      }

      return state;
    }
    return next(ensureErrorReportingEnhancer, initialState, enhancer);
  };
}

module.exports = ensureCSSErrorReportingEnabled;
