"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _lodash = require("devtools/client/shared/vendor/lodash");

var _Breakpoint = require("./Breakpoint");

var _Breakpoint2 = _interopRequireDefault(_Breakpoint);

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _source = require("../../utils/source");

var _selectors = require("../../selectors/index");

var _pause = require("../../utils/pause/index");

var _breakpoint = require("../../utils/breakpoint/index");

var _BreakpointsContextMenu = require("./BreakpointsContextMenu");

var _BreakpointsContextMenu2 = _interopRequireDefault(_BreakpointsContextMenu);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function isCurrentlyPausedAtBreakpoint(frame, why, breakpoint) {
  if (!frame || !(0, _pause.isInterrupted)(why)) {
    return false;
  }

  const bpId = (0, _breakpoint.makeLocationId)(breakpoint.location);
  const pausedId = (0, _breakpoint.makeLocationId)(frame.location);
  return bpId === pausedId;
}

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

function sortFilenames(urlA, urlB) {
  const filenameA = (0, _source.getFilenameFromURL)(urlA);
  const filenameB = (0, _source.getFilenameFromURL)(urlB);

  if (filenameA > filenameB) {
    return 1;
  }

  if (filenameA < filenameB) {
    return -1;
  }

  return 0;
}

class Breakpoints extends _react.Component {
  handleBreakpointCheckbox(breakpoint) {
    if (breakpoint.loading) {
      return;
    }

    if (breakpoint.disabled) {
      this.props.enableBreakpoint(breakpoint.location);
    } else {
      this.props.disableBreakpoint(breakpoint.location);
    }
  }

  selectBreakpoint(breakpoint) {
    this.props.selectLocation(breakpoint.location);
  }

  removeBreakpoint(event, breakpoint) {
    event.stopPropagation();
    this.props.removeBreakpoint(breakpoint.location);
  }

  renderBreakpoint(breakpoint) {
    const {
      selectedSource
    } = this.props;
    return _react2.default.createElement(_Breakpoint2.default, {
      key: breakpoint.locationId,
      breakpoint: breakpoint,
      selectedSource: selectedSource,
      onClick: () => this.selectBreakpoint(breakpoint),
      onContextMenu: e => (0, _BreakpointsContextMenu2.default)(_objectSpread({}, this.props, {
        breakpoint,
        contextMenuEvent: e
      })),
      onChange: () => this.handleBreakpointCheckbox(breakpoint),
      onCloseClick: ev => this.removeBreakpoint(ev, breakpoint)
    });
  }

  renderExceptionsOptions() {
    const {
      breakpoints,
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions
    } = this.props;
    const isEmpty = breakpoints.size == 0;
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
      breakpoints
    } = this.props;

    if (breakpoints.size == 0) {
      return;
    }

    const groupedBreakpoints = (0, _lodash.groupBy)((0, _lodash.sortBy)([...breakpoints.valueSeq()], bp => bp.location.line), bp => (0, _source.getRawSourceURL)(bp.source.url));
    return [...Object.keys(groupedBreakpoints).sort(sortFilenames).map(url => {
      const file = (0, _source.getFilenameFromURL)(url);
      const groupBreakpoints = groupedBreakpoints[url].filter(bp => !bp.hidden && (bp.text || bp.originalText));

      if (!groupBreakpoints.length) {
        return null;
      }

      return [_react2.default.createElement("div", {
        className: "breakpoint-heading",
        title: url,
        key: url,
        onClick: () => this.props.selectSource(groupBreakpoints[0].source.id)
      }, file), ...groupBreakpoints.map(bp => this.renderBreakpoint(bp))];
    })];
  }

  render() {
    return _react2.default.createElement("div", {
      className: "pane breakpoints-list"
    }, this.renderExceptionsOptions(), this.renderBreakpoints());
  }

}

function updateLocation(sources, frame, why, bp) {
  const source = (0, _selectors.getSourceInSources)(sources, bp.location.sourceId);
  const isCurrentlyPaused = isCurrentlyPausedAtBreakpoint(frame, why, bp);
  const locationId = (0, _breakpoint.makeLocationId)(bp.location);

  const localBP = _objectSpread({}, bp, {
    locationId,
    isCurrentlyPaused,
    source
  });

  return localBP;
}

const _getBreakpoints = (0, _reselect.createSelector)(_selectors.getBreakpoints, _selectors.getSources, _selectors.getTopFrame, _selectors.getPauseReason, (breakpoints, sources, frame, why) => breakpoints.map(bp => updateLocation(sources, frame, why, bp)).filter(bp => bp.source && !bp.source.isBlackBoxed));

const mapStateToProps = state => ({
  breakpoints: _getBreakpoints(state),
  selectedSource: (0, _selectors.getSelectedSource)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Breakpoints);