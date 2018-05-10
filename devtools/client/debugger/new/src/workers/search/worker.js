"use strict";

var _getMatches = require("./get-matches");

var _getMatches2 = _interopRequireDefault(_getMatches);

var _projectSearch = require("./project-search");

var _devtoolsUtils = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-utils"];

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  workerHandler
} = _devtoolsUtils.workerUtils;
self.onmessage = workerHandler({
  getMatches: _getMatches2.default,
  findSourceMatches: _projectSearch.findSourceMatches
});