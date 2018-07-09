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

var _Button = require("../shared/Button/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class EventListeners extends _react.Component {
  constructor(...args) {
    super(...args);

    this.renderListener = ({
      type,
      selector,
      line,
      sourceId,
      breakpoint
    }) => {
      const checked = breakpoint && !breakpoint.disabled;
      const location = {
        sourceId,
        line
      };
      return _react2.default.createElement("div", {
        className: "listener",
        onClick: () => this.props.selectLocation({
          sourceId,
          line
        }),
        key: `${type}.${selector}.${sourceId}.${line}`
      }, _react2.default.createElement("input", {
        type: "checkbox",
        className: "listener-checkbox",
        checked: checked,
        onChange: () => this.handleCheckbox(breakpoint, location)
      }), _react2.default.createElement("span", {
        className: "type"
      }, type), _react2.default.createElement("span", {
        className: "selector"
      }, selector), breakpoint ? _react2.default.createElement(_Button.CloseButton, {
        handleClick: ev => this.removeBreakpoint(ev, breakpoint)
      }) : "");
    };
  }

  handleCheckbox(breakpoint, location) {
    if (!breakpoint) {
      return this.props.addBreakpoint(location);
    }

    if (breakpoint.loading) {
      return;
    }

    if (breakpoint.disabled) {
      this.props.enableBreakpoint(breakpoint.location);
    } else {
      this.props.disableBreakpoint(breakpoint.location);
    }
  }

  removeBreakpoint(event, breakpoint) {
    event.stopPropagation();

    if (breakpoint) {
      this.props.removeBreakpoint(breakpoint.location);
    }
  }

  render() {
    const {
      listeners
    } = this.props;
    return _react2.default.createElement("div", {
      className: "pane event-listeners"
    }, listeners.map(this.renderListener));
  }

}

const mapStateToProps = state => {
  const listeners = (0, _selectors.getEventListeners)(state).map(listener => {
    return { ...listener,
      breakpoint: (0, _selectors.getBreakpoint)(state, {
        sourceId: listener.sourceId,
        line: listener.line
      })
    };
  });
  return {
    listeners
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  selectLocation: _actions2.default.selectLocation,
  addBreakpoint: _actions2.default.addBreakpoint,
  enableBreakpoint: _actions2.default.enableBreakpoint,
  disableBreakpoint: _actions2.default.disableBreakpoint,
  removeBreakpoint: _actions2.default.removeBreakpoint
})(EventListeners);