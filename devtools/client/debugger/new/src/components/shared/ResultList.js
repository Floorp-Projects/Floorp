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
class ResultList extends _react.Component {
  constructor(props) {
    super(props);

    _initialiseProps.call(this);
  }

  render() {
    const {
      size,
      items,
      role
    } = this.props;
    return _react2.default.createElement("ul", {
      className: (0, _classnames2.default)("result-list", size),
      id: "result-list",
      role: role,
      "aria-live": "polite"
    }, items.map(this.renderListItem));
  }

}

exports.default = ResultList;
ResultList.defaultProps = {
  size: "small",
  role: "listbox"
};

var _initialiseProps = function () {
  this.renderListItem = (item, index) => {
    if (item.value === "/" && item.title === "") {
      item.title = "(index)";
    }

    const {
      selectItem,
      selected
    } = this.props;
    const props = {
      onClick: event => selectItem(event, item, index),
      key: `${item.id}${item.value}${index}`,
      ref: String(index),
      title: item.value,
      "aria-labelledby": `${item.id}-title`,
      "aria-describedby": `${item.id}-subtitle`,
      role: "option",
      className: (0, _classnames2.default)("result-item", {
        selected: index === selected
      })
    };
    return _react2.default.createElement("li", props, item.icon && _react2.default.createElement("div", null, _react2.default.createElement("img", {
      className: item.icon
    })), _react2.default.createElement("div", {
      id: `${item.id}-title`,
      className: "title"
    }, item.title), _react2.default.createElement("div", {
      id: `${item.id}-subtitle`,
      className: "subtitle"
    }, item.subtitle));
  };
};