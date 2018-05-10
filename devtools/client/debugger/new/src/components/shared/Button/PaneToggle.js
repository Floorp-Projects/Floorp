"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _CommandBarButton = require("./CommandBarButton");

var _CommandBarButton2 = _interopRequireDefault(_CommandBarButton);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class PaneToggleButton extends _react.PureComponent {
  render() {
    const {
      position,
      collapsed,
      horizontal,
      handleClick
    } = this.props;
    const title = !collapsed ? L10N.getStr("expandPanes") : L10N.getStr("collapsePanes");
    return _react2.default.createElement(_CommandBarButton2.default, {
      className: (0, _classnames2.default)("toggle-button", position, {
        collapsed,
        vertical: !horizontal
      }),
      onClick: () => handleClick(position, collapsed),
      title: title
    }, _react2.default.createElement(_Svg2.default, {
      name: "togglePanes"
    }));
  }

}

PaneToggleButton.defaultProps = {
  horizontal: false
};
exports.default = PaneToggleButton;