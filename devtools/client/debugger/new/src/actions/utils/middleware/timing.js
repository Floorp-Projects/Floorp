"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.timing = timing;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global window */

/**
 * Redux middleware that sets performance markers for all actions such that they
 * will appear in performance tooling under the User Timing API
 */
const mark = window.performance && window.performance.mark ? window.performance.mark.bind(window.performance) : () => {};
const measure = window.performance && window.performance.measure ? window.performance.measure.bind(window.performance) : () => {};

function timing(store) {
  return next => action => {
    mark(`${action.type}_start`);
    const result = next(action);
    mark(`${action.type}_end`);
    measure(`${action.type}`, `${action.type}_start`, `${action.type}_end`);
    return result;
  };
}