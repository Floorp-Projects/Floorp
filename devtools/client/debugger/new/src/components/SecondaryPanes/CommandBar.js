/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import PropTypes from "prop-types";
import React, { Component } from "react";

import { connect } from "../../utils/connect";
import classnames from "classnames";
import { features } from "../../utils/prefs";
import {
  getIsPaused,
  getIsWaitingOnBreak,
  getCanRewind,
  getSkipPausing,
  getCurrentThread
} from "../../selectors";
import { formatKeyShortcut } from "../../utils/text";
import actions from "../../actions";
import { debugBtn } from "../shared/Button/CommandBarButton";
import AccessibleImage from "../shared/AccessibleImage";
import "./CommandBar.css";

import { appinfo } from "devtools-services";

const isMacOS = appinfo.OS === "Darwin";

// NOTE: the "resume" command will call either the resume or breakOnNext action
// depending on whether or not the debugger is paused or running
const COMMANDS = ["resume", "stepOver", "stepIn", "stepOut"];

const KEYS = {
  WINNT: {
    resume: "F8",
    stepOver: "F10",
    stepIn: "F11",
    stepOut: "Shift+F11"
  },
  Darwin: {
    resume: "Cmd+\\",
    stepOver: "Cmd+'",
    stepIn: "Cmd+;",
    stepOut: "Cmd+Shift+:",
    stepOutDisplay: "Cmd+Shift+;"
  },
  Linux: {
    resume: "F8",
    stepOver: "F10",
    stepIn: "F11",
    stepOut: "Shift+F11"
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
    const winKey =
      getKeyForOS("WINNT", `${action}Display`) || getKeyForOS("WINNT", action);
    // display both Windows type and Mac specific keys
    return formatKeyShortcut([key, winKey].join(" "));
  }
  return formatKeyShortcut(key);
}

type Props = {
  isPaused: boolean,
  isWaitingOnBreak: boolean,
  horizontal: boolean,
  canRewind: boolean,
  skipPausing: boolean,
  resume: typeof actions.resume,
  stepIn: typeof actions.stepIn,
  stepOut: typeof actions.stepOut,
  stepOver: typeof actions.stepOver,
  breakOnNext: typeof actions.breakOnNext,
  rewind: typeof actions.rewind,
  reverseStepIn: typeof actions.reverseStepIn,
  reverseStepOut: typeof actions.reverseStepOut,
  reverseStepOver: typeof actions.reverseStepOver,
  pauseOnExceptions: typeof actions.pauseOnExceptions,
  toggleSkipPausing: typeof actions.toggleSkipPausing
};

class CommandBar extends Component<Props> {
  componentWillUnmount() {
    const shortcuts = this.context.shortcuts;
    COMMANDS.forEach(action => shortcuts.off(getKey(action)));
    if (isMacOS) {
      COMMANDS.forEach(action => shortcuts.off(getKeyForOS("WINNT", action)));
    }
  }

  componentDidMount() {
    const shortcuts = this.context.shortcuts;

    COMMANDS.forEach(action =>
      shortcuts.on(getKey(action), (_, e) => this.handleEvent(e, action))
    );

    if (isMacOS) {
      // The Mac supports both the Windows Function keys
      // as well as the Mac non-Function keys
      COMMANDS.forEach(action =>
        shortcuts.on(getKeyForOS("WINNT", action), (_, e) =>
          this.handleEvent(e, action)
        )
      );
    }
  }

  handleEvent(e, action) {
    e.preventDefault();
    e.stopPropagation();
    if (action === "resume") {
      this.props.isPaused ? this.props.resume() : this.props.breakOnNext();
    } else {
      this.props[action]();
    }
  }

  renderStepButtons() {
    const { isPaused, canRewind } = this.props;
    const className = isPaused ? "active" : "disabled";
    const isDisabled = !isPaused;

    if (canRewind || (!isPaused && features.removeCommandBarOptions)) {
      return;
    }

    return [
      debugBtn(
        this.props.stepOver,
        "stepOver",
        className,
        L10N.getFormatStr("stepOverTooltip", formatKey("stepOver")),
        isDisabled
      ),
      debugBtn(
        this.props.stepIn,
        "stepIn",
        className,
        L10N.getFormatStr("stepInTooltip", formatKey("stepIn")),
        isDisabled
      ),
      debugBtn(
        this.props.stepOut,
        "stepOut",
        className,
        L10N.getFormatStr("stepOutTooltip", formatKey("stepOut")),
        isDisabled
      )
    ];
  }

  resume() {
    this.props.resume();
  }

  renderPauseButton() {
    const { isPaused, breakOnNext, isWaitingOnBreak, canRewind } = this.props;

    if (isPaused) {
      if (canRewind) {
        return null;
      }
      return debugBtn(
        () => this.resume(),
        "resume",
        "active",
        L10N.getFormatStr("resumeButtonTooltip", formatKey("resume"))
      );
    }

    if (features.removeCommandBarOptions && !this.props.canRewind) {
      return;
    }

    if (isWaitingOnBreak) {
      return debugBtn(
        null,
        "pause",
        "disabled",
        L10N.getStr("pausePendingButtonTooltip"),
        true
      );
    }

    return debugBtn(
      breakOnNext,
      "pause",
      "active",
      L10N.getFormatStr("pauseButtonTooltip", formatKey("resume"))
    );
  }

  renderTimeTravelButtons() {
    const { isPaused, canRewind } = this.props;

    if (!canRewind || !isPaused) {
      return null;
    }

    const isDisabled = !isPaused;

    return [
      debugBtn(this.props.rewind, "rewind", "active", "Rewind Execution"),

      debugBtn(
        this.props.resume,
        "resume",
        "active",
        L10N.getFormatStr("resumeButtonTooltip", formatKey("resume"))
      ),
      <div key="divider-1" className="divider" />,
      debugBtn(
        this.props.reverseStepOver,
        "reverseStepOver",
        "active",
        "Reverse step over"
      ),
      debugBtn(
        this.props.stepOver,
        "stepOver",
        "active",
        L10N.getFormatStr("stepOverTooltip", formatKey("stepOver")),
        isDisabled
      ),
      <div key="divider-2" className="divider" />,
      debugBtn(
        this.props.stepOut,
        "stepOut",
        "active",
        L10N.getFormatStr("stepOutTooltip", formatKey("stepOut")),
        isDisabled
      ),

      debugBtn(
        this.props.stepIn,
        "stepIn",
        "active",
        L10N.getFormatStr("stepInTooltip", formatKey("stepIn")),
        isDisabled
      )
    ];
  }

  renderSkipPausingButton() {
    const { skipPausing, toggleSkipPausing } = this.props;

    if (!features.skipPausing) {
      return null;
    }

    return (
      <button
        className={classnames(
          "command-bar-button",
          "command-bar-skip-pausing",
          {
            active: skipPausing
          }
        )}
        title={L10N.getStr("skipPausingTooltip.label")}
        onClick={toggleSkipPausing}
      >
        <AccessibleImage className="disable-pausing" />
      </button>
    );
  }

  render() {
    return (
      <div
        className={classnames("command-bar", {
          vertical: !this.props.horizontal
        })}
      >
        {this.renderPauseButton()}
        {this.renderStepButtons()}

        {this.renderTimeTravelButtons()}
        <div className="filler" />
        {this.renderSkipPausingButton()}
      </div>
    );
  }
}

CommandBar.contextTypes = {
  shortcuts: PropTypes.object
};

const mapStateToProps = state => ({
  isPaused: getIsPaused(state, getCurrentThread(state)),
  isWaitingOnBreak: getIsWaitingOnBreak(state, getCurrentThread(state)),
  canRewind: getCanRewind(state),
  skipPausing: getSkipPausing(state)
});

export default connect(
  mapStateToProps,
  {
    resume: actions.resume,
    stepIn: actions.stepIn,
    stepOut: actions.stepOut,
    stepOver: actions.stepOver,
    breakOnNext: actions.breakOnNext,
    rewind: actions.rewind,
    reverseStepIn: actions.reverseStepIn,
    reverseStepOut: actions.reverseStepOut,
    reverseStepOver: actions.reverseStepOver,
    pauseOnExceptions: actions.pauseOnExceptions,
    toggleSkipPausing: actions.toggleSkipPausing
  }
)(CommandBar);
