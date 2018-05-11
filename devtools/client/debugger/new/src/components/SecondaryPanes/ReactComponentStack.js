"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _selectors = require("../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class ReactComponentStack extends _react.PureComponent {
  onMouseDown(e, componentIndex) {
    if (e.nativeEvent.which == 3) {
      return;
    }

    this.props.selectComponent(componentIndex);
  }

  render() {
    const {
      componentStack
    } = this.props.extra.react;
    return _react2.default.createElement("div", {
      className: "pane frames"
    }, _react2.default.createElement("ul", null, componentStack.slice().reverse().map((component, index) => _react2.default.createElement("li", {
      className: (0, _classnames2.default)("frame", {
        selected: this.props.selectedComponentIndex === index
      }),
      key: index,
      onMouseDown: e => this.onMouseDown(e, index)
    }, component))));
  }

}

const mapStateToProps = state => ({
  extra: (0, _selectors.getExtra)(state),
  selectedComponentIndex: (0, _selectors.getSelectedComponentIndex)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(ReactComponentStack);