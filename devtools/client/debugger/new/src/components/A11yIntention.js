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
class A11yIntention extends _react2.default.Component {
  constructor(...args) {
    var _temp;

    return _temp = super(...args), this.state = {
      keyboard: false
    }, this.handleKeyDown = () => {
      this.setState({
        keyboard: true
      });
    }, this.handleMouseDown = () => {
      this.setState({
        keyboard: false
      });
    }, _temp;
  }

  render() {
    return _react2.default.createElement("div", {
      className: this.state.keyboard ? "A11y-keyboard" : "A11y-mouse",
      onKeyDown: this.handleKeyDown,
      onMouseDown: this.handleMouseDown
    }, this.props.children);
  }

}

exports.default = A11yIntention;