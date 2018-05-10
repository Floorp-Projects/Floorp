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

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _editor = require("../../utils/editor/index");

var _prefs = require("../../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const breakpointSvg = document.createElement("div");

_reactDom2.default.render(_react2.default.createElement(_Svg2.default, {
  name: "breakpoint"
}), breakpointSvg);

function makeMarker(isDisabled) {
  const bp = breakpointSvg.cloneNode(true);
  bp.className = (0, _classnames2.default)("editor new-breakpoint", {
    "breakpoint-disabled": isDisabled,
    "folding-enabled": _prefs.features.codeFolding
  });
  return bp;
}

class Breakpoint extends _react.Component {
  constructor() {
    super();

    this.addBreakpoint = () => {
      const {
        breakpoint,
        editor,
        selectedSource
      } = this.props; // Hidden Breakpoints are never rendered on the client

      if (breakpoint.hidden) {
        return;
      } // NOTE: we need to wait for the breakpoint to be loaded
      // to get the generated location


      if (!selectedSource || breakpoint.loading) {
        return;
      }

      const sourceId = selectedSource.id;
      const line = (0, _editor.toEditorLine)(sourceId, breakpoint.location.line);
      editor.codeMirror.setGutterMarker(line, "breakpoints", makeMarker(breakpoint.disabled));
      editor.codeMirror.addLineClass(line, "line", "new-breakpoint");

      if (breakpoint.condition) {
        editor.codeMirror.addLineClass(line, "line", "has-condition");
      } else {
        editor.codeMirror.removeLineClass(line, "line", "has-condition");
      }
    };
  }

  shouldComponentUpdate(nextProps) {
    const {
      editor,
      breakpoint,
      selectedSource
    } = this.props;
    return editor !== nextProps.editor || breakpoint.disabled !== nextProps.breakpoint.disabled || breakpoint.hidden !== nextProps.breakpoint.hidden || breakpoint.condition !== nextProps.breakpoint.condition || breakpoint.loading !== nextProps.breakpoint.loading || selectedSource !== nextProps.selectedSource;
  }

  componentDidMount() {
    this.addBreakpoint();
  }

  componentDidUpdate() {
    this.addBreakpoint();
  }

  componentWillUnmount() {
    const {
      editor,
      breakpoint,
      selectedSource
    } = this.props;

    if (!selectedSource) {
      return;
    }

    if (breakpoint.loading) {
      return;
    }

    const sourceId = selectedSource.id;
    const doc = (0, _editor.getDocument)(sourceId);

    if (!doc) {
      return;
    }

    const line = (0, _editor.toEditorLine)(sourceId, breakpoint.location.line); // NOTE: when we upgrade codemirror we can use `doc.setGutterMarker`

    if (doc.setGutterMarker) {
      doc.setGutterMarker(line, "breakpoints", null);
    } else {
      editor.codeMirror.setGutterMarker(line, "breakpoints", null);
    }

    doc.removeLineClass(line, "line", "new-breakpoint");
    doc.removeLineClass(line, "line", "has-condition");
  }

  render() {
    return null;
  }

}

exports.default = Breakpoint;