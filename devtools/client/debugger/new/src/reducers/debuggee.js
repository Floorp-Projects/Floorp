"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getWorkers = exports.createDebuggeeState = undefined;
exports.default = debuggee;
exports.getWorker = getWorker;

var _immutable = require("devtools/client/shared/vendor/immutable");

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Debuggee reducer
 * @module reducers/debuggee
 */
const createDebuggeeState = exports.createDebuggeeState = (0, _makeRecord2.default)({
  workers: (0, _immutable.List)()
});

function debuggee(state = createDebuggeeState(), action) {
  switch (action.type) {
    case "SET_WORKERS":
      return state.set("workers", (0, _immutable.List)(action.workers));

    default:
      return state;
  }
}

const getWorkers = exports.getWorkers = state => state.debuggee.workers;

function getWorker(state, url) {
  return getWorkers(state).find(value => url);
}