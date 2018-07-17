"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.replaceOriginalVariableName = exports.getPausePoints = exports.getFramework = exports.mapExpression = exports.hasSyntaxError = exports.clearSources = exports.setSource = exports.hasSource = exports.getNextStep = exports.clearASTs = exports.clearScopes = exports.clearSymbols = exports.findOutOfScopeLocations = exports.getScopes = exports.getSymbols = exports.getClosestExpression = exports.stop = exports.start = undefined;

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
const getClosestExpression = exports.getClosestExpression = dispatcher.task("getClosestExpression");
const getSymbols = exports.getSymbols = dispatcher.task("getSymbols");
const getScopes = exports.getScopes = dispatcher.task("getScopes");
const findOutOfScopeLocations = exports.findOutOfScopeLocations = dispatcher.task("findOutOfScopeLocations");
const clearSymbols = exports.clearSymbols = dispatcher.task("clearSymbols");
const clearScopes = exports.clearScopes = dispatcher.task("clearScopes");
const clearASTs = exports.clearASTs = dispatcher.task("clearASTs");
const getNextStep = exports.getNextStep = dispatcher.task("getNextStep");
const hasSource = exports.hasSource = dispatcher.task("hasSource");
const setSource = exports.setSource = dispatcher.task("setSource");
const clearSources = exports.clearSources = dispatcher.task("clearSources");
const hasSyntaxError = exports.hasSyntaxError = dispatcher.task("hasSyntaxError");
const mapExpression = exports.mapExpression = dispatcher.task("mapExpression");
const getFramework = exports.getFramework = dispatcher.task("getFramework");
const getPausePoints = exports.getPausePoints = dispatcher.task("getPausePoints");
const replaceOriginalVariableName = exports.replaceOriginalVariableName = dispatcher.task("replaceOriginalVariableName");