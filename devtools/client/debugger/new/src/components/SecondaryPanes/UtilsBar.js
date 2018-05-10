"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _CommandBarButton = require("../shared/Button/CommandBarButton");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class UtilsBar extends _react.Component {
  renderUtilButtons() {
    return [(0, _CommandBarButton.debugBtn)(this.props.toggleShortcutsModal, "shortcuts", "active", L10N.getStr("shortcuts.buttonName"), false)];
  }

  render() {
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("command-bar bottom", {
        vertical: !this.props.horizontal
      })
    }, this.renderUtilButtons());
  }

}

exports.default = UtilsBar;