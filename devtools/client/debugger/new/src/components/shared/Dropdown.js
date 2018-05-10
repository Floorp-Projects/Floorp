"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.Dropdown = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Dropdown extends _react.Component {
  constructor(props) {
    super(props);

    this.toggleDropdown = e => {
      this.setState(prevState => ({
        dropdownShown: !prevState.dropdownShown
      }));
    };

    this.state = {
      dropdownShown: false
    };
  }

  renderPanel() {
    return _react2.default.createElement("div", {
      className: "dropdown",
      onClick: this.toggleDropdown,
      style: {
        display: this.state.dropdownShown ? "block" : "none"
      }
    }, this.props.panel);
  }

  renderButton() {
    return _react2.default.createElement("button", {
      className: "dropdown-button",
      onClick: this.toggleDropdown
    }, this.props.icon);
  }

  renderMask() {
    return _react2.default.createElement("div", {
      className: "dropdown-mask",
      onClick: this.toggleDropdown,
      style: {
        display: this.state.dropdownShown ? "block" : "none"
      }
    });
  }

  render() {
    return _react2.default.createElement("div", {
      className: "dropdown-block"
    }, this.renderPanel(), this.renderButton(), this.renderMask());
  }

}

exports.Dropdown = Dropdown;
exports.default = Dropdown;