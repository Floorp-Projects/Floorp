"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.findSourceMatches = exports.getMatches = exports.stop = exports.start = undefined;

var _devtoolsUtils = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-utils"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  WorkerDispatcher
} = _devtoolsUtils.workerUtils;
const dispatcher = new WorkerDispatcher();
const start = exports.start = dispatcher.start.bind(dispatcher);
const stop = exports.stop = dispatcher.stop.bind(dispatcher);
const getMatches = exports.getMatches = dispatcher.task("getMatches");
const findSourceMatches = exports.findSourceMatches = dispatcher.task("findSourceMatches");