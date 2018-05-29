"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _actions = require("../../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _BreakpointsContextMenu = require("./BreakpointsContextMenu");

var _BreakpointsContextMenu2 = _interopRequireDefault(_BreakpointsContextMenu);

var _Button = require("../../shared/Button/index");

var _breakpoint = require("../../../utils/breakpoint/index");

var _prefs = require("../../../utils/prefs");

var _editor = require("../../../utils/editor/index");

var _selectors = require("../../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function getMappedLocation(mappedLocation, selectedSource) {
  return selectedSource && (0, _devtoolsSourceMap.isGeneratedId)(selectedSource.id) ? mappedLocation.generatedLocation : mappedLocation.location;
}

class Breakpoint extends _react.PureComponent {
  constructor(...args) {
    var _temp;

    return _temp = super(...args), this.onContextMenu = e => {
      (0, _BreakpointsContextMenu2.default)(_objectSpread({}, this.props, {
        contextMenuEvent: e
      }));
    }, this.selectBreakpoint = () => {
      const {
        breakpoint,
        selectSpecificLocation
      } = this.props;
      selectSpecificLocation(breakpoint.location);
    }, this.removeBreakpoint = event => {
      const {
        breakpoint,
        removeBreakpoint
      } = this.props;
      event.stopPropagation();
      removeBreakpoint(breakpoint.location);
    }, this.handleBreakpointCheckbox = () => {
      const {
        breakpoint,
        enableBreakpoint,
        disableBreakpoint
      } = this.props;

      if (breakpoint.loading) {
        return;
      }

      if (breakpoint.disabled) {
        enableBreakpoint(breakpoint.location);
      } else {
        disableBreakpoint(breakpoint.location);
      }
    }, _temp;
  }

  isCurrentlyPausedAtBreakpoint() {
    const {
      frame,
      breakpoint,
      selectedSource
    } = this.props;

    if (!frame) {
      return false;
    }

    const bpId = (0, _breakpoint.getLocationWithoutColumn)(getMappedLocation(breakpoint, selectedSource));
    const frameId = (0, _breakpoint.getLocationWithoutColumn)(getMappedLocation(frame, selectedSource));
    return bpId == frameId;
  }

  getBreakpointLocation() {
    const {
      breakpoint,
      source,
      selectedSource
    } = this.props;
    const {
      column,
      line
    } = getMappedLocation(breakpoint, selectedSource);
    const isWasm = source && source.isWasm;
    const columnVal = _prefs.features.columnBreakpoints && column ? `:${column}` : "";
    const bpLocation = isWasm ? `0x${line.toString(16).toUpperCase()}` : `${line}${columnVal}`;
    return bpLocation;
  }

  getBreakpointText() {
    const {
      selectedSource,
      breakpoint
    } = this.props;
    const {
      condition
    } = breakpoint;

    if (condition) {
      return condition;
    }

    if (selectedSource && (0, _devtoolsSourceMap.isGeneratedId)(selectedSource.id)) {
      return breakpoint.text;
    }

    return breakpoint.originalText;
  }

  highlightText() {
    const text = this.getBreakpointText();
    const codeMirror = (0, _editor.getCodeMirror)();

    if (!text || !codeMirror) {
      return {
        __html: ""
      };
    }

    const node = document.createElement("div");
    codeMirror.constructor.runMode(text, "application/javascript", node);
    return {
      __html: node.innerHTML
    };
  }

  render() {
    const {
      breakpoint
    } = this.props;
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)({
        breakpoint,
        paused: this.isCurrentlyPausedAtBreakpoint(),
        disabled: breakpoint.disabled,
        "is-conditional": !!breakpoint.condition
      }),
      onClick: this.selectBreakpoint,
      onContextMenu: this.onContextMenu
    }, _react2.default.createElement("input", {
      type: "checkbox",
      className: "breakpoint-checkbox",
      checked: !breakpoint.disabled,
      onChange: this.handleBreakpointCheckbox,
      onClick: ev => ev.stopPropagation()
    }), _react2.default.createElement("label", {
      className: "breakpoint-label cm-s-mozilla",
      title: this.getBreakpointText(),
      dangerouslySetInnerHTML: this.highlightText()
    }), _react2.default.createElement("div", {
      className: "breakpoint-line-close"
    }, _react2.default.createElement("div", {
      className: "breakpoint-line"
    }, this.getBreakpointLocation()), _react2.default.createElement(_Button.CloseButton, {
      handleClick: e => this.removeBreakpoint(e),
      tooltip: L10N.getStr("breakpoints.removeBreakpointTooltip")
    })));
  }

}

const mapStateToProps = state => ({
  frame: (0, _selectors.getTopFrame)(state),
  selectedSource: (0, _selectors.getSelectedSource)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Breakpoint);