"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getPausePoints = exports.getFramework = exports.mapExpression = exports.hasSyntaxError = exports.clearSources = exports.setSource = exports.hasSource = exports.getSymbols = exports.clearSymbols = exports.clearScopes = exports.getScopes = exports.clearASTs = exports.getNextStep = exports.findOutOfScopeLocations = exports.stop = exports.start = undefined;

var _devtoolsUtils = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-utils"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  WorkerDispatcher
} = _devtoolsUtils.workerUtils;
const dispatcher = new WorkerDispatcher();

const start = exports.start = (url, win=window) => dispatcher.start(url, win);

const stop = exports.stop = () => dispatcher.stop();

const findOutOfScopeLocations = exports.findOutOfScopeLocations = async (sourceId, position) => dispatcher.invoke("findOutOfScopeLocations", sourceId, position);

const getNextStep = exports.getNextStep = async (sourceId, pausedPosition) => dispatcher.invoke("getNextStep", sourceId, pausedPosition);

const clearASTs = exports.clearASTs = async () => dispatcher.invoke("clearASTs");

const getScopes = exports.getScopes = async location => dispatcher.invoke("getScopes", location);

const clearScopes = exports.clearScopes = async () => dispatcher.invoke("clearScopes");

const clearSymbols = exports.clearSymbols = async () => dispatcher.invoke("clearSymbols");

const getSymbols = exports.getSymbols = async sourceId => dispatcher.invoke("getSymbols", sourceId);

const hasSource = exports.hasSource = async sourceId => dispatcher.invoke("hasSource", sourceId);

const setSource = exports.setSource = async source => dispatcher.invoke("setSource", source);

const clearSources = exports.clearSources = async () => dispatcher.invoke("clearSources");

const hasSyntaxError = exports.hasSyntaxError = async input => dispatcher.invoke("hasSyntaxError", input);

const mapExpression = exports.mapExpression = async (expression, mappings, bindings, shouldMapBindings, shouldMapAwait) => dispatcher.invoke("mapExpression", expression, mappings, bindings, shouldMapBindings, shouldMapAwait);

const getFramework = exports.getFramework = async sourceId => dispatcher.invoke("getFramework", sourceId);

const getPausePoints = exports.getPausePoints = async sourceId => dispatcher.invoke("getPausePoints", sourceId);