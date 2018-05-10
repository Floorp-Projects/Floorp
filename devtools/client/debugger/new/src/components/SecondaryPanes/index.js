"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _propTypes = require("devtools/client/shared/vendor/react-prop-types");

var _propTypes2 = _interopRequireDefault(_propTypes);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _immutable = require("devtools/client/shared/vendor/immutable");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _prefs = require("../../utils/prefs");

var _Breakpoints = require("./Breakpoints");

var _Breakpoints2 = _interopRequireDefault(_Breakpoints);

var _Expressions = require("./Expressions");

var _Expressions2 = _interopRequireDefault(_Expressions);

var _devtoolsSplitter = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-splitter"];

var _devtoolsSplitter2 = _interopRequireDefault(_devtoolsSplitter);

var _Frames = require("./Frames/index");

var _Frames2 = _interopRequireDefault(_Frames);

var _EventListeners = require("./EventListeners");

var _EventListeners2 = _interopRequireDefault(_EventListeners);

var _Workers = require("./Workers");

var _Workers2 = _interopRequireDefault(_Workers);

var _Accordion = require("../shared/Accordion");

var _Accordion2 = _interopRequireDefault(_Accordion);

var _CommandBar = require("./CommandBar");

var _CommandBar2 = _interopRequireDefault(_CommandBar);

var _UtilsBar = require("./UtilsBar");

var _UtilsBar2 = _interopRequireDefault(_UtilsBar);

var _FrameworkComponent = require("./FrameworkComponent");

var _FrameworkComponent2 = _interopRequireDefault(_FrameworkComponent);

var _ReactComponentStack = require("./ReactComponentStack");

var _ReactComponentStack2 = _interopRequireDefault(_ReactComponentStack);

var _Scopes = require("./Scopes");

var _Scopes2 = _interopRequireDefault(_Scopes);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function debugBtn(onClick, type, className, tooltip) {
  return _react2.default.createElement("button", {
    onClick: onClick,
    className: `${type} ${className}`,
    key: type,
    title: tooltip
  }, _react2.default.createElement(_Svg2.default, {
    name: type,
    title: tooltip,
    "aria-label": tooltip
  }));
}

class SecondaryPanes extends _react.Component {
  constructor(props) {
    super(props);

    this.onExpressionAdded = () => {
      this.setState({
        showExpressionsInput: false
      });
    };

    this.state = {
      showExpressionsInput: false
    };
  }

  renderBreakpointsToggle() {
    const {
      toggleAllBreakpoints,
      breakpoints,
      breakpointsDisabled,
      breakpointsLoading
    } = this.props;
    const isIndeterminate = !breakpointsDisabled && breakpoints.some(x => x.disabled);

    if (_prefs.features.skipPausing || breakpoints.size == 0) {
      return null;
    }

    const inputProps = {
      type: "checkbox",
      "aria-label": breakpointsDisabled ? L10N.getStr("breakpoints.enable") : L10N.getStr("breakpoints.disable"),
      className: "breakpoints-toggle",
      disabled: breakpointsLoading,
      key: "breakpoints-toggle",
      onChange: e => {
        e.stopPropagation();
        toggleAllBreakpoints(!breakpointsDisabled);
      },
      onClick: e => e.stopPropagation(),
      checked: !breakpointsDisabled && !isIndeterminate,
      ref: input => {
        if (input) {
          input.indeterminate = isIndeterminate;
        }
      },
      title: breakpointsDisabled ? L10N.getStr("breakpoints.enable") : L10N.getStr("breakpoints.disable")
    };
    return _react2.default.createElement("input", inputProps);
  }

  watchExpressionHeaderButtons() {
    const {
      expressions
    } = this.props;
    const buttons = [];

    if (expressions.size) {
      buttons.push(debugBtn(evt => {
        evt.stopPropagation();
        this.props.evaluateExpressions();
      }, "refresh", "refresh", L10N.getStr("watchExpressions.refreshButton")));
    }

    buttons.push(debugBtn(evt => {
      if (_prefs.prefs.expressionsVisible) {
        evt.stopPropagation();
      }

      this.setState({
        showExpressionsInput: true
      });
    }, "plus", "plus", L10N.getStr("expressions.placeholder")));
    return buttons;
  }

  getScopeItem() {
    return {
      header: L10N.getStr("scopes.header"),
      className: "scopes-pane",
      component: _react2.default.createElement(_Scopes2.default, null),
      opened: _prefs.prefs.scopesVisible,
      onToggle: opened => {
        _prefs.prefs.scopesVisible = opened;
      }
    };
  }

  getComponentStackItem() {
    return {
      header: L10N.getStr("components.header"),
      component: _react2.default.createElement(_ReactComponentStack2.default, null),
      opened: _prefs.prefs.componentStackVisible,
      onToggle: opened => {
        _prefs.prefs.componentStackVisible = opened;
      }
    };
  }

  getComponentItem() {
    const {
      extra: {
        react
      }
    } = this.props;
    return {
      header: react.displayName,
      className: "component-pane",
      component: _react2.default.createElement(_FrameworkComponent2.default, null),
      opened: _prefs.prefs.componentVisible,
      onToggle: opened => {
        _prefs.prefs.componentVisible = opened;
      }
    };
  }

  getWatchItem() {
    return {
      header: L10N.getStr("watchExpressions.header"),
      className: "watch-expressions-pane",
      buttons: this.watchExpressionHeaderButtons(),
      component: _react2.default.createElement(_Expressions2.default, {
        showInput: this.state.showExpressionsInput,
        onExpressionAdded: this.onExpressionAdded
      }),
      opened: _prefs.prefs.expressionsVisible,
      onToggle: opened => {
        _prefs.prefs.expressionsVisible = opened;
      }
    };
  }

  getCallStackItem() {
    return {
      header: L10N.getStr("callStack.header"),
      className: "call-stack-pane",
      component: _react2.default.createElement(_Frames2.default, null),
      opened: _prefs.prefs.callStackVisible,
      onToggle: opened => {
        _prefs.prefs.callStackVisible = opened;
      }
    };
  }

  getWorkersItem() {
    return {
      header: L10N.getStr("workersHeader"),
      className: "workers-pane",
      component: _react2.default.createElement(_Workers2.default, null),
      opened: _prefs.prefs.workersVisible,
      onToggle: opened => {
        _prefs.prefs.workersVisible = opened;
      }
    };
  }

  getBreakpointsItem() {
    const {
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions
    } = this.props;
    return {
      header: L10N.getStr("breakpoints.header"),
      className: "breakpoints-pane",
      buttons: [this.renderBreakpointsToggle()],
      component: _react2.default.createElement(_Breakpoints2.default, {
        shouldPauseOnExceptions: shouldPauseOnExceptions,
        shouldPauseOnCaughtExceptions: shouldPauseOnCaughtExceptions,
        pauseOnExceptions: pauseOnExceptions
      }),
      opened: _prefs.prefs.breakpointsVisible,
      onToggle: opened => {
        _prefs.prefs.breakpointsVisible = opened;
      }
    };
  }

  getStartItems() {
    const {
      extra,
      workers
    } = this.props;
    const items = [];

    if (this.props.horizontal) {
      if (_prefs.features.workers && workers.size > 0) {
        items.push(this.getWorkersItem());
      }

      items.push(this.getWatchItem());
    }

    items.push(this.getBreakpointsItem());

    if (this.props.hasFrames) {
      items.push(this.getCallStackItem());

      if (this.props.horizontal) {
        if (extra && extra.react) {
          if (_prefs.features.componentStack && extra.react.componentStack.length > 1) {
            items.push(this.getComponentStackItem());
          }

          items.push(this.getComponentItem());
        }

        items.push(this.getScopeItem());
      }
    }

    if (_prefs.features.eventListeners) {
      items.push({
        header: L10N.getStr("eventListenersHeader"),
        className: "event-listeners-pane",
        component: _react2.default.createElement(_EventListeners2.default, null)
      });
    }

    return items.filter(item => item);
  }

  renderHorizontalLayout() {
    return _react2.default.createElement(_Accordion2.default, {
      items: this.getItems()
    });
  }

  getEndItems() {
    const {
      extra,
      workers
    } = this.props;
    let items = [];

    if (this.props.horizontal) {
      return [];
    }

    if (_prefs.features.workers && workers.size > 0) {
      items.push(this.getWorkersItem());
    }

    items.push(this.getWatchItem());

    if (extra && extra.react) {
      items.push(this.getComponentItem());
    }

    if (this.props.hasFrames) {
      items = [...items, this.getScopeItem()];
    }

    return items;
  }

  getItems() {
    return [...this.getStartItems(), ...this.getEndItems()];
  }

  renderVerticalLayout() {
    return _react2.default.createElement(_devtoolsSplitter2.default, {
      initialSize: "300px",
      minSize: 10,
      maxSize: "50%",
      splitterSize: 1,
      startPanel: _react2.default.createElement(_Accordion2.default, {
        items: this.getStartItems()
      }),
      endPanel: _react2.default.createElement(_Accordion2.default, {
        items: this.getEndItems()
      })
    });
  }

  renderUtilsBar() {
    if (!_prefs.features.shortcuts) {
      return;
    }

    return _react2.default.createElement(_UtilsBar2.default, {
      horizontal: this.props.horizontal,
      toggleShortcutsModal: this.props.toggleShortcutsModal
    });
  }

  render() {
    return _react2.default.createElement("div", {
      className: "secondary-panes-wrapper"
    }, _react2.default.createElement(_CommandBar2.default, {
      horizontal: this.props.horizontal
    }), _react2.default.createElement("div", {
      className: "secondary-panes"
    }, this.props.horizontal ? this.renderHorizontalLayout() : this.renderVerticalLayout()), this.renderUtilsBar());
  }

}

SecondaryPanes.contextTypes = {
  shortcuts: _propTypes2.default.object
};

const mapStateToProps = state => ({
  expressions: (0, _selectors.getExpressions)(state),
  extra: (0, _selectors.getExtra)(state),
  hasFrames: !!(0, _selectors.getTopFrame)(state),
  breakpoints: (0, _selectors.getBreakpoints)(state),
  breakpointsDisabled: (0, _selectors.getBreakpointsDisabled)(state),
  breakpointsLoading: (0, _selectors.getBreakpointsLoading)(state),
  isWaitingOnBreak: (0, _selectors.getIsWaitingOnBreak)(state),
  shouldPauseOnExceptions: (0, _selectors.getShouldPauseOnExceptions)(state),
  shouldPauseOnCaughtExceptions: (0, _selectors.getShouldPauseOnCaughtExceptions)(state),
  workers: (0, _selectors.getWorkers)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(SecondaryPanes);