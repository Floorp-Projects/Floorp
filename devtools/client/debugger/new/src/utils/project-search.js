"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.highlightMatches = highlightMatches;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// Maybe reuse file search's functions?
function highlightMatches(lineMatch) {
  const {
    value,
    column,
    match
  } = lineMatch;
  const len = match.length;
  return _react2.default.createElement("span", {
    className: "line-value"
  }, _react2.default.createElement("span", {
    className: "line-match",
    key: 0
  }, value.slice(0, column)), _react2.default.createElement("span", {
    className: "query-match",
    key: 1
  }, value.substr(column, len)), _react2.default.createElement("span", {
    className: "line-match",
    key: 2
  }, value.slice(column + len, value.length)));
}