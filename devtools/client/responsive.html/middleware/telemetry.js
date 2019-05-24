/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { debounce } = require("devtools/shared/debounce");
const {
  RESIZE_VIEWPORT,
  ROTATE_VIEWPORT,
} = require("../actions/index");
const TELEMETRY_SCALAR_VIEWPORT_CHANGE_COUNT =
  "devtools.responsive.viewport_change_count";

/**
 * Redux middleware to observe actions dispatched to the Redux store and log to Telemetry.
 *
 * `telemetryMiddleware()` is a wrapper function which receives the Telemetry
 * instance as its only argument and is used to create a closure to make that instance
 * available to the returned Redux middleware and functions nested within.
 *
 * To be used with `applyMiddleware()` helper when creating the Redux store.
 * @see https://redux.js.org/api/applymiddleware
 *
 * The wrapper returns the function that acts as the Redux middleware. This function
 * receives a single `store` argument (the Redux Middleware API, an object containing
 * `getState()` and `dispatch()`) and returns a function which receives a single `next`
 * argument (the next middleware in the chain) which itself returns a function that
 * receives a single `action` argument (the dispatched action).
 *
 * @param  {Object} telemetry
 *         Instance of the Telemetry API
 * @return {Function}
 */
function telemetryMiddleware(telemetry) {
  function logViewportChange() {
    telemetry.scalarAdd(TELEMETRY_SCALAR_VIEWPORT_CHANGE_COUNT, 1);
  }

  // Debounced logging to use in response to high frequency actions like RESIZE_VIEWPORT.
  // Set debounce()'s `immediate` parameter to `true` to ensure the very first
  // call is executed immediately. This helps the tests to check that logging is working
  // without adding needless complexity to wait for the debounced call.
  const logViewportChangeDebounced = debounce(logViewportChange, 300, null, true);

  // This cascade of functions is the Redux middleware signature.
  // @see https://redux.js.org/api/applymiddleware#arguments
  return store => next => action => {
    const res = next(action);
    // Pass through to the next middleware if a telemetry instance is not available, for
    // example when running unit tests.
    if (!telemetry) {
      return res;
    }

    switch (action.type) {
      case ROTATE_VIEWPORT:
        logViewportChange();
        break;
      case RESIZE_VIEWPORT:
        logViewportChangeDebounced();
        break;
    }

    return res;
  };
}

module.exports = telemetryMiddleware;
