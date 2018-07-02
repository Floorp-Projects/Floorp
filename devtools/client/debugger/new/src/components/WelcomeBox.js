"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.WelcomeBox = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _actions = require("../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../selectors/index");

var _text = require("../utils/text");

var _Button = require("./shared/Button/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class WelcomeBox extends _react.Component {
  renderToggleButton() {
    const {
      horizontal,
      endPanelCollapsed,
      togglePaneCollapse
    } = this.props;

    if (horizontal) {
      return;
    }

    return _react2.default.createElement(_Button.PaneToggleButton, {
      position: "end",
      collapsed: !endPanelCollapsed,
      horizontal: horizontal,
      handleClick: togglePaneCollapse
    });
  }

  render() {
    const searchSourcesShortcut = (0, _text.formatKeyShortcut)(L10N.getStr("sources.search.key2"));
    const searchProjectShortcut = (0, _text.formatKeyShortcut)(L10N.getStr("projectTextSearch.key"));
    const searchSourcesLabel = L10N.getStr("welcome.search").substring(2);
    const searchProjectLabel = L10N.getStr("welcome.findInFiles").substring(2);
    const {
      setActiveSearch,
      openQuickOpen
    } = this.props;
    return _react2.default.createElement("div", {
      className: "welcomebox"
    }, _react2.default.createElement("div", {
      className: "alignlabel"
    }, _react2.default.createElement("div", {
      className: "shortcutFunction"
    }, _react2.default.createElement("p", {
      className: "welcomebox__searchSources",
      role: "button",
      tabIndex: "0",
      onClick: () => openQuickOpen()
    }, _react2.default.createElement("span", {
      className: "shortcutKey"
    }, searchSourcesShortcut), _react2.default.createElement("span", {
      className: "shortcutLabel"
    }, searchSourcesLabel)), _react2.default.createElement("p", {
      className: "welcomebox__searchProject",
      role: "button",
      tabIndex: "0",
      onClick: setActiveSearch.bind(null, "project")
    }, _react2.default.createElement("span", {
      className: "shortcutKey"
    }, searchProjectShortcut), _react2.default.createElement("span", {
      className: "shortcutLabel"
    }, searchProjectLabel))), this.renderToggleButton()));
  }

}

exports.WelcomeBox = WelcomeBox;

const mapStateToProps = state => ({
  endPanelCollapsed: (0, _selectors.getPaneCollapse)(state, "end")
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  togglePaneCollapse: _actions2.default.togglePaneCollapse,
  setActiveSearch: _actions2.default.setActiveSearch,
  openQuickOpen: _actions2.default.openQuickOpen
})(WelcomeBox);