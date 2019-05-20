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
  getIsWaitingOnBreak,
  getCanRewind,
  getSkipPausing,
  getCurrentThread,
  getThreadContext,
} from "../../selectors";
import { formatKeyShortcut } from "../../utils/text";
import actions from "../../actions";
import { debugBtn } from "../shared/Button/CommandBarButton";
import AccessibleImage from "../shared/AccessibleImage";
import "./CommandBar.css";

import { appinfo } from "devtools-services";
import type { ThreadContext } from "../../types";

const isMacOS = appinfo.OS === "Darwin";

// NOTE: the "resume" command will call either the resume or breakOnNext action
// depending on whether or not the debugger is paused or running
const COMMANDS = ["resume", "stepOver", "stepIn", "stepOut"];

const KEYS = {
  WINNT: {
    resume: "F8",
    stepOver: "F10",
    stepIn: "F11",
    stepOut: "Shift+F11",
  },
  Darwin: {
    resume: "Cmd+\\",
    stepOver: "Cmd+'",
    stepIn: "Cmd+;",
    stepOut: "Cmd+Shift+:",
    stepOutDisplay: "Cmd+Shift+;",
  },
  Linux: {
    resume: "F8",
    stepOver: "F10",
    stepIn: "F11",
    stepOut: "Shift+F11",
  },
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
  cx: ThreadContext,
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
  reverseStepOver: typeof actions.reverseStepOver,
  pauseOnExceptions: typeof actions.pauseOnExceptions,
  toggleSkipPausing: typeof actions.toggleSkipPausing,
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
    const { cx } = this.props;
    e.preventDefault();
    e.stopPropagation();
    if (action === "resume") {
      this.props.cx.isPaused
        ? this.props.resume(cx)
        : this.props.breakOnNext(cx);
    } else {
      this.props[action](cx);
    }
  }

  renderStepButtons() {
    const { cx, canRewind } = this.props;
    const className = cx.isPaused ? "active" : "disabled";
    const isDisabled = !cx.isPaused;

    if (canRewind || (!cx.isPaused && features.removeCommandBarOptions)) {
      return;
    }

    return [
      debugBtn(
        () => this.props.stepOver(cx),
        "stepOver",
        className,
        L10N.getFormatStr("stepOverTooltip", formatKey("stepOver")),
        isDisabled
      ),
      debugBtn(
        () => this.props.stepIn(cx),
        "stepIn",
        className,
        L10N.getFormatStr("stepInTooltip", formatKey("stepIn")),
        isDisabled
      ),
      debugBtn(
        () => this.props.stepOut(cx),
        "stepOut",
        className,
        L10N.getFormatStr("stepOutTooltip", formatKey("stepOut")),
        isDisabled
      ),
    ];
  }

  resume() {
    this.props.resume(this.props.cx);
  }

  renderPauseButton() {
    const { cx, breakOnNext, isWaitingOnBreak, canRewind } = this.props;

    if (cx.isPaused) {
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
      () => breakOnNext(cx),
      "pause",
      "active",
      L10N.getFormatStr("pauseButtonTooltip", formatKey("resume"))
    );
  }

  renderTimeTravelButtons() {
    const { cx, canRewind } = this.props;

    if (!canRewind || !cx.isPaused) {
      return null;
    }

    const isDisabled = !cx.isPaused;

    return [
      debugBtn(
        () => this.props.rewind(cx),
        "rewind",
        "active",
        "Rewind Execution"
      ),

      debugBtn(
        () => this.props.resume(cx),
        "resume",
        "active",
        L10N.getFormatStr("resumeButtonTooltip", formatKey("resume"))
      ),
      <div key="divider-1" className="divider" />,
      debugBtn(
        () => this.props.reverseStepOver(cx),
        "reverseStepOver",
        "active",
        "Reverse step over"
      ),
      debugBtn(
        () => this.props.stepOver(cx),
        "stepOver",
        "active",
        L10N.getFormatStr("stepOverTooltip", formatKey("stepOver")),
        isDisabled
      ),
      <div key="divider-2" className="divider" />,
      debugBtn(
        () => this.props.stepOut(cx),
        "stepOut",
        "active",
        L10N.getFormatStr("stepOutTooltip", formatKey("stepOut")),
        isDisabled
      ),

      debugBtn(
        () => this.props.stepIn(cx),
        "stepIn",
        "active",
        L10N.getFormatStr("stepInTooltip", formatKey("stepIn")),
        isDisabled
      ),
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
            active: skipPausing,
          }
        )}
        title={
          skipPausing
            ? L10N.getStr("undoSkipPausingTooltip.label")
            : L10N.getStr("skipPausingTooltip.label")
        }
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
          vertical: !this.props.horizontal,
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
  shortcuts: PropTypes.object,
};

const mapStateToProps = state => ({
  cx: getThreadContext(state),
  isWaitingOnBreak: getIsWaitingOnBreak(state, getCurrentThread(state)),
  canRewind: getCanRewind(state),
  skipPausing: getSkipPausing(state),
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
    reverseStepOver: actions.reverseStepOver,
    pauseOnExceptions: actions.pauseOnExceptions,
    toggleSkipPausing: actions.toggleSkipPausing,
  }
)(CommandBar);
