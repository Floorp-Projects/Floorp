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

var _Close = require("../shared/Button/Close");

var _Close2 = _interopRequireDefault(_Close);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

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
      }, selector), breakpoint ? _react2.default.createElement(_Close2.default, {
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
    return _objectSpread({}, listener, {
      breakpoint: (0, _selectors.getBreakpoint)(state, {
        sourceId: listener.sourceId,
        line: listener.line,
        column: null
      })
    });
  });
  return {
    listeners
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(EventListeners);