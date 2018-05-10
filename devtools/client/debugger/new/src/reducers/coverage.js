"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createCoverageState = undefined;
exports.getHitCountForSource = getHitCountForSource;
exports.getCoverageEnabled = getCoverageEnabled;

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _fromJS = require("../utils/fromJS");

var _fromJS2 = _interopRequireDefault(_fromJS);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Code coverage reducer
 * @module reducers/coverage
 */
const createCoverageState = exports.createCoverageState = (0, _makeRecord2.default)({
  coverageOn: false,
  hitCount: I.Map()
});

function update(state = createCoverageState(), action) {
  switch (action.type) {
    case "RECORD_COVERAGE":
      return state.mergeIn(["hitCount"], (0, _fromJS2.default)(action.value.coverage)).setIn(["coverageOn"], true);

    default:
      {
        return state;
      }
  }
} // NOTE: we'd like to have the app state fully typed
// https://github.com/devtools-html/debugger.html/blob/master/src/reducers/sources.js#L179-L185


function getHitCountForSource(state, sourceId) {
  const hitCount = state.coverage.get("hitCount");
  return hitCount.get(sourceId);
}

function getCoverageEnabled(state) {
  return state.coverage.get("coverageOn");
}

exports.default = update;