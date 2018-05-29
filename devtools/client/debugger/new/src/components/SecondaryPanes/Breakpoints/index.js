"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _Breakpoint = require("./Breakpoint");

var _Breakpoint2 = _interopRequireDefault(_Breakpoint);

var _SourceIcon = require("../../shared/SourceIcon");

var _SourceIcon2 = _interopRequireDefault(_SourceIcon);

var _actions = require("../../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _source = require("../../../utils/source");

var _breakpoint = require("../../../utils/breakpoint/index");

var _selectors = require("../../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function createExceptionOption(label, value, onChange, className) {
  return _react2.default.createElement("div", {
    className: className,
    onClick: onChange
  }, _react2.default.createElement("input", {
    type: "checkbox",
    checked: value ? "checked" : "",
    onChange: e => e.stopPropagation() && onChange()
  }), _react2.default.createElement("div", {
    className: "breakpoint-exceptions-label"
  }, label));
}

class Breakpoints extends _react.Component {
  renderExceptionsOptions() {
    const {
      breakpointSources,
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions
    } = this.props;
    const isEmpty = breakpointSources.length == 0;
    const exceptionsBox = createExceptionOption(L10N.getStr("pauseOnExceptionsItem2"), shouldPauseOnExceptions, () => pauseOnExceptions(!shouldPauseOnExceptions, false), "breakpoints-exceptions");
    const ignoreCaughtBox = createExceptionOption(L10N.getStr("pauseOnCaughtExceptionsItem"), shouldPauseOnCaughtExceptions, () => pauseOnExceptions(true, !shouldPauseOnCaughtExceptions), "breakpoints-exceptions-caught");
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("breakpoints-exceptions-options", {
        empty: isEmpty
      })
    }, exceptionsBox, shouldPauseOnExceptions ? ignoreCaughtBox : null);
  }

  renderBreakpoints() {
    const {
      breakpointSources
    } = this.props;
    return [...breakpointSources.map(({
      source,
      breakpoints
    }) => [_react2.default.createElement("div", {
      className: "breakpoint-heading",
      title: source.url,
      key: source.url,
      onClick: () => this.props.selectSource(source.id)
    }, _react2.default.createElement(_SourceIcon2.default, {
      source: source
    }), (0, _source.getFilename)(source)), ...breakpoints.map(breakpoint => _react2.default.createElement(_Breakpoint2.default, {
      breakpoint: breakpoint,
      source: source,
      key: (0, _breakpoint.makeLocationId)(breakpoint.location)
    }))])];
  }

  render() {
    return _react2.default.createElement("div", {
      className: "pane breakpoints-list"
    }, this.renderExceptionsOptions(), this.renderBreakpoints());
  }

}

const mapStateToProps = state => ({
  breakpointSources: (0, _selectors.getBreakpointSources)(state),
  selectedSource: (0, _selectors.getSelectedSource)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Breakpoints);