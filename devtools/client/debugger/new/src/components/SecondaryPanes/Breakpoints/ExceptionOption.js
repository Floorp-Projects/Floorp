"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = ExceptionOption;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function ExceptionOption({
  className,
  isChecked = false,
  label,
  onChange
}) {
  return _react2.default.createElement("div", {
    className: className,
    onClick: onChange
  }, _react2.default.createElement("input", {
    type: "checkbox",
    checked: isChecked ? "checked" : "",
    onChange: e => e.stopPropagation() && onChange()
  }), _react2.default.createElement("div", {
    className: "breakpoint-exceptions-label"
  }, label));
}