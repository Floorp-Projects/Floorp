"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _CallSite = require("./CallSite");

var _CallSite2 = _interopRequireDefault(_CallSite);

var _selectors = require("../../selectors/index");

var _editor = require("../../utils/editor/index");

var _wasm = require("../../utils/wasm");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getCallSiteAtLocation(callSites, location) {
  return callSites.find(callSite => (0, _lodash.isEqualWith)(callSite.location, location, (cloc, loc) => {
    return loc.line === cloc.start.line && loc.column >= cloc.start.column && loc.column <= cloc.end.column;
  }));
}

class CallSites extends _react.Component {
  constructor(props) {
    super(props);

    this.onKeyUp = e => {
      if (e.key === "Alt") {
        e.preventDefault();
        this.setState({
          showCallSites: false
        });
      }
    };

    this.onKeyDown = e => {
      if (e.key === "Alt") {
        e.preventDefault();
        this.setState({
          showCallSites: true
        });
      }
    };

    this.state = {
      showCallSites: false
    };
  }

  componentDidMount() {
    const {
      editor
    } = this.props;
    const codeMirrorWrapper = editor.codeMirror.getWrapperElement();
    codeMirrorWrapper.addEventListener("click", e => this.onTokenClick(e));
    document.body.addEventListener("keydown", this.onKeyDown);
    document.body.addEventListener("keyup", this.onKeyUp);
  }

  componentWillUnmount() {
    const {
      editor
    } = this.props;
    const codeMirrorWrapper = editor.codeMirror.getWrapperElement();
    codeMirrorWrapper.removeEventListener("click", e => this.onTokenClick(e));
    document.body.removeEventListener("keydown", this.onKeyDown);
    document.body.removeEventListener("keyup", this.onKeyUp);
  }

  onTokenClick(e) {
    const {
      target
    } = e;
    const {
      editor,
      selectedLocation
    } = this.props;

    if (!e.altKey && !target.classList.contains("call-site-bp") || !target.classList.contains("call-site") && !target.classList.contains("call-site-bp")) {
      return;
    }

    const {
      sourceId
    } = selectedLocation;
    const {
      line,
      column
    } = (0, _editor.getTokenLocation)(editor.codeMirror, target);
    this.toggleBreakpoint(line, (0, _wasm.isWasm)(sourceId) ? undefined : column);
  }

  toggleBreakpoint(line, column = undefined) {
    const {
      selectedSource,
      selectedLocation,
      addBreakpoint,
      removeBreakpoint,
      callSites
    } = this.props;
    const callSite = getCallSiteAtLocation(callSites, {
      line,
      column
    });

    if (!callSite) {
      return;
    }

    const bp = callSite.breakpoint;

    if (bp && bp.loading || !selectedLocation || !selectedSource) {
      return;
    }

    const {
      sourceId
    } = selectedLocation;

    if (bp) {
      // NOTE: it's possible the breakpoint has slid to a column
      column = column || bp.location.column;
      removeBreakpoint({
        sourceId: sourceId,
        line: line,
        column
      });
    } else {
      addBreakpoint({
        sourceId: sourceId,
        sourceUrl: selectedSource.url,
        line: line,
        column: column
      });
    }
  }

  render() {
    const {
      editor,
      callSites,
      selectedSource
    } = this.props;
    const {
      showCallSites
    } = this.state;
    let sites;

    if (!callSites) {
      return null;
    }

    editor.codeMirror.operation(() => {
      const childCallSites = callSites.map((callSite, index) => {
        const props = {
          key: index,
          callSite,
          editor,
          source: selectedSource,
          breakpoint: callSite.breakpoint,
          showCallSite: showCallSites
        };
        return _react2.default.createElement(_CallSite2.default, props);
      });
      sites = _react2.default.createElement("div", null, childCallSites);
    });
    return sites;
  }

}

function getCallSites(symbols, breakpoints) {
  if (!symbols || !symbols.callExpressions) {
    return;
  }

  const callSites = symbols.callExpressions; // NOTE: we create a breakpoint map keyed on location
  // to speed up the lookups. Hopefully we'll fix the
  // inconsistency with column offsets so that we can expect
  // a breakpoint to be added at the beginning of a call expression.

  const bpLocationMap = (0, _lodash.keyBy)(breakpoints.valueSeq().toJS(), ({
    location
  }) => locationKey(location));

  function locationKey({
    line,
    column
  }) {
    return `${line}/${column}`;
  }

  function findBreakpoint(callSite) {
    const {
      location: {
        start,
        end
      }
    } = callSite;
    const breakpointId = (0, _lodash.range)(start.column - 1, end.column).map(column => locationKey({
      line: start.line,
      column
    })).find(key => bpLocationMap[key]);

    if (breakpointId) {
      return bpLocationMap[breakpointId];
    }
  }

  return callSites.filter(({
    location
  }) => location.start.line === location.end.line).map(callSite => ({ ...callSite,
    breakpoint: findBreakpoint(callSite)
  }));
}

const mapStateToProps = state => {
  const selectedLocation = (0, _selectors.getSelectedLocation)(state);
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const sourceId = selectedLocation && selectedLocation.sourceId;
  const symbols = (0, _selectors.getSymbols)(state, selectedSource);
  const breakpoints = (0, _selectors.getBreakpointsForSource)(state, sourceId);
  return {
    selectedLocation,
    selectedSource,
    callSites: getCallSites(symbols, breakpoints),
    breakpoints: breakpoints
  };
};

const {
  addBreakpoint,
  removeBreakpoint
} = _actions2.default;
const mapDispatchToProps = {
  addBreakpoint,
  removeBreakpoint
};
exports.default = (0, _reactRedux.connect)(mapStateToProps, mapDispatchToProps)(CallSites);