"use strict";

var _getSymbols = require("./getSymbols");

var _ast = require("./utils/ast");

var _getScopes = require("./getScopes/index");

var _getScopes2 = _interopRequireDefault(_getScopes);

var _sources = require("./sources");

var _findOutOfScopeLocations = require("./findOutOfScopeLocations");

var _findOutOfScopeLocations2 = _interopRequireDefault(_findOutOfScopeLocations);

var _steps = require("./steps");

var _validate = require("./validate");

var _frameworks = require("./frameworks");

var _pausePoints = require("./pausePoints");

var _mapOriginalExpression = require("./mapOriginalExpression");

var _mapOriginalExpression2 = _interopRequireDefault(_mapOriginalExpression);

var _devtoolsUtils = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-utils"];

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  workerHandler
} = _devtoolsUtils.workerUtils;
self.onmessage = workerHandler({
  findOutOfScopeLocations: _findOutOfScopeLocations2.default,
  getSymbols: _getSymbols.getSymbols,
  getScopes: _getScopes2.default,
  clearSymbols: _getSymbols.clearSymbols,
  clearScopes: _getScopes.clearScopes,
  clearASTs: _ast.clearASTs,
  hasSource: _sources.hasSource,
  setSource: _sources.setSource,
  clearSources: _sources.clearSources,
  getNextStep: _steps.getNextStep,
  hasSyntaxError: _validate.hasSyntaxError,
  getFramework: _frameworks.getFramework,
  getPausePoints: _pausePoints.getPausePoints,
  mapOriginalExpression: _mapOriginalExpression2.default
});