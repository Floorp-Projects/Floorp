"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactDom = require("devtools/client/shared/vendor/react-dom");

var _reactDom2 = _interopRequireDefault(_reactDom);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _Close = require("../shared/Button/Close");

var _Close2 = _interopRequireDefault(_Close);

var _breakpoint = require("../../utils/breakpoint/index");

var _prefs = require("../../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getBreakpointLocation(source, line, column) {
  const isWasm = source && source.isWasm;
  const columnVal = _prefs.features.columnBreakpoints && column ? `:${column}` : "";
  const bpLocation = isWasm ? `0x${line.toString(16).toUpperCase()}` : `${line}${columnVal}`;
  return bpLocation;
}

function getBreakpointText(selectedSource, breakpoint) {
  const {
    condition,
    text,
    originalText
  } = breakpoint;

  if (condition) {
    return condition;
  }

  if (!selectedSource || (0, _devtoolsSourceMap.isGeneratedId)(selectedSource.id) || originalText.length == 0) {
    return text;
  }

  return originalText;
}

class Breakpoint extends _react.Component {
  componentDidMount() {
    this.setupEditor();
  }

  componentDidUpdate(prevProps) {
    if (getBreakpointText(this.props.selectedSource, this.props.breakpoint) != getBreakpointText(prevProps.selectedSource, prevProps.breakpoint)) {
      this.destroyEditor();
    }

    this.setupEditor();
  }

  componentWillUnmount() {
    this.destroyEditor();
  }

  shouldComponentUpdate(nextProps) {
    const prevBreakpoint = this.props.breakpoint;
    const nextBreakpoint = nextProps.breakpoint;
    return !prevBreakpoint || this.props.selectedSource != nextProps.selectedSource || prevBreakpoint.text != nextBreakpoint.text || prevBreakpoint.disabled != nextBreakpoint.disabled || prevBreakpoint.condition != nextBreakpoint.condition || prevBreakpoint.hidden != nextBreakpoint.hidden || prevBreakpoint.isCurrentlyPaused != nextBreakpoint.isCurrentlyPaused;
  }

  destroyEditor() {
    if (this.editor) {
      this.editor.destroy();
      this.editor = null;
    }
  }

  setupEditor() {
    if (this.editor) {
      return;
    }

    const {
      selectedSource,
      breakpoint
    } = this.props;
    this.editor = (0, _breakpoint.createEditor)(getBreakpointText(selectedSource, breakpoint)); // disables the default search shortcuts
    // $FlowIgnore

    this.editor._initShortcuts = () => {};

    const node = _reactDom2.default.findDOMNode(this);

    if (node instanceof HTMLElement) {
      const mountNode = node.querySelector(".breakpoint-label");

      if (node instanceof HTMLElement) {
        // $FlowIgnore
        mountNode.innerHTML = "";
        this.editor.appendToLocalElement(mountNode);
        this.editor.codeMirror.on("mousedown", (_, e) => e.preventDefault());
      }
    }
  }

  renderCheckbox() {
    const {
      onChange,
      breakpoint
    } = this.props;
    const {
      disabled
    } = breakpoint;
    return _react2.default.createElement("input", {
      type: "checkbox",
      className: "breakpoint-checkbox",
      checked: !disabled,
      onChange: onChange,
      onClick: ev => ev.stopPropagation()
    });
  }

  renderText() {
    const {
      selectedSource,
      breakpoint
    } = this.props;
    const text = getBreakpointText(selectedSource, breakpoint);
    return _react2.default.createElement("label", {
      className: "breakpoint-label",
      title: text
    }, text);
  }

  renderLineClose() {
    const {
      breakpoint,
      onCloseClick,
      selectedSource
    } = this.props;
    const {
      location
    } = breakpoint;
    let {
      line,
      column
    } = location;

    if (selectedSource && (0, _devtoolsSourceMap.isGeneratedId)(selectedSource.id) && breakpoint.generatedLocation) {
      line = breakpoint.generatedLocation.line;
      column = breakpoint.generatedLocation.column;
    }

    return _react2.default.createElement("div", {
      className: "breakpoint-line-close"
    }, _react2.default.createElement("div", {
      className: "breakpoint-line"
    }, getBreakpointLocation(breakpoint.source, line, column)), _react2.default.createElement(_Close2.default, {
      handleClick: onCloseClick,
      tooltip: L10N.getStr("breakpoints.removeBreakpointTooltip")
    }));
  }

  render() {
    const {
      breakpoint,
      onClick,
      onContextMenu
    } = this.props;
    const locationId = breakpoint.locationId;
    const isCurrentlyPaused = breakpoint.isCurrentlyPaused;
    const isDisabled = breakpoint.disabled;
    const isConditional = !!breakpoint.condition;
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)({
        breakpoint,
        paused: isCurrentlyPaused,
        disabled: isDisabled,
        "is-conditional": isConditional
      }),
      key: locationId,
      onClick: onClick,
      onContextMenu: onContextMenu
    }, this.renderCheckbox(), this.renderText(), this.renderLineClose());
  }

}

exports.default = Breakpoint;