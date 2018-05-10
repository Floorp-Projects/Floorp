"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Accordion extends _react.Component {
  constructor(props) {
    super(props);

    this.renderContainer = (item, i) => {
      const {
        opened
      } = item;
      return _react2.default.createElement("li", {
        role: "listitem",
        className: item.className,
        key: i
      }, _react2.default.createElement("h2", {
        className: "_header",
        tabIndex: "0",
        onClick: () => this.handleHeaderClick(i)
      }, _react2.default.createElement(_Svg2.default, {
        name: "arrow",
        className: opened ? "expanded" : ""
      }), item.header, item.buttons ? _react2.default.createElement("div", {
        className: "header-buttons",
        tabIndex: "-1"
      }, item.buttons) : null), opened && _react2.default.createElement("div", {
        className: "_content"
      }, (0, _react.cloneElement)(item.component, item.componentProps || {})));
    };

    this.state = {
      opened: props.items.map(item => item.opened),
      created: []
    };
  }

  handleHeaderClick(i) {
    const item = this.props.items[i];
    const opened = !item.opened;
    item.opened = opened;

    if (item.onToggle) {
      item.onToggle(opened);
    } // We force an update because otherwise the accordion
    // would not re-render


    this.forceUpdate();
  }

  render() {
    return _react2.default.createElement("ul", {
      role: "list",
      className: "accordion"
    }, this.props.items.map(this.renderContainer));
  }

}

exports.default = Accordion;