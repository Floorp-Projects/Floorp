/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/* global window */

/**
 * Redux middleware that sets performance markers for all actions such that they
 * will appear in performance tooling under the User Timing API
 */

const mark =
  window.performance && window.performance.mark
    ? window.performance.mark.bind(window.performance)
    : a => {};

const measure =
  window.performance && window.performance.measure
    ? window.performance.measure.bind(window.performance)
    : (a, b, c) => {};

export function timing(store: any) {
  return (next: any) => (action: any) => {
    mark(`${action.type}_start`);
    const result = next(action);
    mark(`${action.type}_end`);
    measure(`${action.type}`, `${action.type}_start`, `${action.type}_end`);
    return result;
  };
}
