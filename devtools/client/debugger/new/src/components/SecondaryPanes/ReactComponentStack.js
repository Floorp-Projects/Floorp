"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class ReactComponentStack extends _react.PureComponent {
  render() {
    const {
      componentStack
    } = this.props.extra.react;
    return _react2.default.createElement("div", {
      className: "pane frames"
    }, _react2.default.createElement("ul", null, componentStack.slice().reverse().map((component, index) => _react2.default.createElement("li", {
      key: index
    }, component))));
  }

}

const mapStateToProps = state => ({
  extra: (0, _selectors.getExtra)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(ReactComponentStack);