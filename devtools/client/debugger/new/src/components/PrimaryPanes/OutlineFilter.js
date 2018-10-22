"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class OutlineFilter extends _react.Component {
  constructor(...args) {
    var _temp;

    return _temp = super(...args), this.state = {
      focused: false
    }, this.setFocus = shouldFocus => {
      this.setState({
        focused: shouldFocus
      });
    }, this.onChange = e => {
      this.props.updateFilter(e.target.value);
    }, this.onKeyDown = e => {
      if (e.key === "Escape" && this.props.filter !== "") {
        // use preventDefault to override toggling the split-console which is
        // also bound to the ESC key
        e.preventDefault();
        this.props.updateFilter("");
      }
    }, _temp;
  }

  render() {
    const {
      focused
    } = this.state;
    return _react2.default.createElement("div", {
      className: "outline-filter"
    }, _react2.default.createElement("form", null, _react2.default.createElement("input", {
      className: (0, _classnames2.default)("outline-filter-input", {
        focused
      }),
      onFocus: () => this.setFocus(true),
      onBlur: () => this.setFocus(false),
      placeholder: L10N.getStr("outline.placeholder"),
      value: this.props.filter,
      type: "text",
      onChange: this.onChange,
      onKeyDown: this.onKeyDown
    })));
  }

}

exports.default = OutlineFilter;