"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function CloseButton({
  handleClick,
  buttonClass,
  tooltip
}) {
  return _react2.default.createElement("button", {
    className: buttonClass ? `close-btn ${buttonClass}` : "close-btn",
    onClick: handleClick,
    title: tooltip
  }, _react2.default.createElement("img", {
    className: "close"
  }));
}

exports.default = CloseButton;