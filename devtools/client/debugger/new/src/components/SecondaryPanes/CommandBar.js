"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _propTypes = require("devtools/client/shared/vendor/react-prop-types");

var _propTypes2 = _interopRequireDefault(_propTypes);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _prefs = require("../../utils/prefs");

var _selectors = require("../../selectors/index");

var _text = require("../../utils/text");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _CommandBarButton = require("../shared/Button/CommandBarButton");

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  appinfo
} = _devtoolsModules.Services;
const isMacOS = appinfo.OS === "Darwin";
const COMMANDS = ["resume", "stepOver", "stepIn", "stepOut"];
const KEYS = {
  WINNT: {
    resume: "F8",
    pause: "F8",
    stepOver: "F10",
    stepIn: "F11",
    stepOut: "Shift+F11"
  },
  Darwin: {
    resume: "Cmd+\\",
    pause: "Cmd+\\",
    stepOver: "Cmd+'",
    stepIn: "Cmd+;",
    stepOut: "Cmd+Shift+:",
    stepOutDisplay: "Cmd+Shift+;"
  },
  Linux: {
    resume: "F8",
    pause: "F8",
    stepOver: "F10",
    stepIn: "Ctrl+F11",
    stepOut: "Ctrl+Shift+F11"
  }
};

function getKey(action) {
  return getKeyForOS(appinfo.OS, action);
}

function getKeyForOS(os, action) {
  const osActions = KEYS[os] || KEYS.Linux;
  return osActions[action];
}

function formatKey(action) {
  const key = getKey(`${action}Display`) || getKey(action);

  if (isMacOS) {
    const winKey = getKeyForOS("WINNT", `${action}Display`) || getKeyForOS("WINNT", action); // display both Windows type and Mac specific keys

    return (0, _text.formatKeyShortcut)([key, winKey].join(" "));
  }

  return (0, _text.formatKeyShortcut)(key);
}

class CommandBar extends _react.Component {
  componentWillUnmount() {
    const shortcuts = this.context.shortcuts;
    COMMANDS.forEach(action => shortcuts.off(getKey(action)));

    if (isMacOS) {
      COMMANDS.forEach(action => shortcuts.off(getKeyForOS("WINNT", action)));
    }
  }

  componentDidMount() {
    const shortcuts = this.context.shortcuts;
    COMMANDS.forEach(action => shortcuts.on(getKey(action), (_, e) => this.handleEvent(e, action)));

    if (isMacOS) {
      // The Mac supports both the Windows Function keys
      // as well as the Mac non-Function keys
      COMMANDS.forEach(action => shortcuts.on(getKeyForOS("WINNT", action), (_, e) => this.handleEvent(e, action)));
    }
  }

  handleEvent(e, action) {
    e.preventDefault();
    e.stopPropagation();
    this.props[action]();
  }

  setHistory(offset) {
    this.props.timeTravelTo(this.props.historyPosition + offset);
  }

  renderStepButtons() {
    const {
      isPaused,
      canRewind
    } = this.props;
    const className = isPaused ? "active" : "disabled";
    const isDisabled = !isPaused;

    if (canRewind || !isPaused && _prefs.features.removeCommandBarOptions) {
      return;
    }

    return [(0, _CommandBarButton.debugBtn)(this.props.stepOver, "stepOver", className, L10N.getFormatStr("stepOverTooltip", formatKey("stepOver")), isDisabled), (0, _CommandBarButton.debugBtn)(this.props.stepIn, "stepIn", className, L10N.getFormatStr("stepInTooltip", formatKey("stepIn")), isDisabled), (0, _CommandBarButton.debugBtn)(this.props.stepOut, "stepOut", className, L10N.getFormatStr("stepOutTooltip", formatKey("stepOut")), isDisabled)];
  }

  resume() {
    this.props.resume();
    this.props.clearHistory();
  }

  renderPauseButton() {
    const {
      isPaused,
      breakOnNext,
      isWaitingOnBreak,
      canRewind
    } = this.props;

    if (canRewind) {
      return;
    }

    if (isPaused) {
      return (0, _CommandBarButton.debugBtn)(() => this.resume(), "resume", "active", L10N.getFormatStr("resumeButtonTooltip", formatKey("resume")));
    }

    if (_prefs.features.removeCommandBarOptions && !this.props.canRewind) {
      return;
    }

    if (isWaitingOnBreak) {
      return (0, _CommandBarButton.debugBtn)(null, "pause", "disabled", L10N.getStr("pausePendingButtonTooltip"), true);
    }

    return (0, _CommandBarButton.debugBtn)(breakOnNext, "pause", "active", L10N.getFormatStr("pauseButtonTooltip", formatKey("pause")));
  }

  renderTimeTravelButtons() {
    const {
      isPaused,
      canRewind
    } = this.props;

    if (!canRewind || !isPaused) {
      return null;
    }

    const isDisabled = !isPaused;
    return [(0, _CommandBarButton.debugBtn)(this.props.rewind, "rewind", "active", "Rewind Execution"), (0, _CommandBarButton.debugBtn)(() => this.props.resume, "resume", "active", L10N.getFormatStr("resumeButtonTooltip", formatKey("resume"))), _react2.default.createElement("div", {
      key: "divider-1",
      className: "divider"
    }), (0, _CommandBarButton.debugBtn)(this.props.reverseStepOver, "reverseStepOver", "active", "Reverse step over"), (0, _CommandBarButton.debugBtn)(this.props.stepOver, "stepOver", "active", L10N.getFormatStr("stepOverTooltip", formatKey("stepOver")), isDisabled), _react2.default.createElement("div", {
      key: "divider-2",
      className: "divider"
    }), (0, _CommandBarButton.debugBtn)(this.props.stepOut, "stepOut", "active", L10N.getFormatStr("stepOutTooltip", formatKey("stepOut")), isDisabled), (0, _CommandBarButton.debugBtn)(this.props.stepIn, "stepIn", "active", L10N.getFormatStr("stepInTooltip", formatKey("stepIn")), isDisabled)];
  }

  replayPreviousButton() {
    const {
      history,
      historyPosition,
      canRewind
    } = this.props;
    const historyLength = history.length;

    if (canRewind || !historyLength || historyLength <= 1 || !_prefs.features.replay) {
      return null;
    }

    const enabled = historyPosition === 0;
    const activeClass = enabled ? "replay-inactive" : "";
    return (0, _CommandBarButton.debugBtn)(() => this.setHistory(-1), `replay-previous ${activeClass}`, "active", L10N.getStr("replayPrevious"), enabled);
  }

  replayNextButton() {
    const {
      history,
      historyPosition,
      canRewind
    } = this.props;
    const historyLength = history.length;

    if (canRewind || !historyLength || historyLength <= 1 || !_prefs.features.replay) {
      return null;
    }

    const enabled = historyPosition + 1 === historyLength;
    const activeClass = enabled ? "replay-inactive" : "";
    return (0, _CommandBarButton.debugBtn)(() => this.setHistory(1), `replay-next ${activeClass}`, "active", L10N.getStr("replayNext"), enabled);
  }

  renderStepPosition() {
    const {
      history,
      historyPosition,
      canRewind
    } = this.props;
    const historyLength = history.length;

    if (canRewind || !historyLength || !_prefs.features.replay) {
      return null;
    }

    const position = historyPosition + 1;
    const total = historyLength;
    const activePrev = position > 1 ? "replay-active" : "replay-inactive";
    const activeNext = position < total ? "replay-active" : "replay-inactive";
    return _react2.default.createElement("div", {
      className: "step-position"
    }, _react2.default.createElement("span", {
      className: activePrev
    }, position), _react2.default.createElement("span", null, " | "), _react2.default.createElement("span", {
      className: activeNext
    }, total));
  }

  renderSkipPausingButton() {
    const {
      skipPausing,
      toggleSkipPausing
    } = this.props;

    if (!_prefs.features.skipPausing) {
      return null;
    }

    return _react2.default.createElement("button", {
      className: (0, _classnames2.default)("command-bar-button", {
        active: skipPausing
      }),
      title: L10N.getStr("skipPausingTooltip"),
      onClick: toggleSkipPausing
    }, _react2.default.createElement("img", {
      className: "skipPausing"
    }));
  }

  render() {
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("command-bar", {
        vertical: !this.props.horizontal
      })
    }, this.renderPauseButton(), this.renderStepButtons(), this.renderTimeTravelButtons(), _react2.default.createElement("div", {
      className: "filler"
    }), this.replayPreviousButton(), this.renderStepPosition(), this.replayNextButton(), this.renderSkipPausingButton());
  }

}

CommandBar.contextTypes = {
  shortcuts: _propTypes2.default.object
};

const mapStateToProps = state => ({
  isPaused: (0, _selectors.isPaused)(state),
  history: (0, _selectors.getHistory)(state),
  historyPosition: (0, _selectors.getHistoryPosition)(state),
  isWaitingOnBreak: (0, _selectors.getIsWaitingOnBreak)(state),
  canRewind: (0, _selectors.getCanRewind)(state),
  skipPausing: (0, _selectors.getSkipPausing)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(CommandBar);