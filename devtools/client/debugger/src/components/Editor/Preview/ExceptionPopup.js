/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "../../../utils/connect";
import classnames from "classnames";

import Reps from "devtools-reps";
const {
  REPS: { StringRep },
} = Reps;

import actions from "../../../actions";

import { getThreadContext } from "../../../selectors";

import AccessibleImage from "../../shared/AccessibleImage";
import { DevToolsUtils } from "devtools-modules";

import type { ThreadContext, StacktraceFrame, Exception } from "../../../types";

type Props = {
  cx: ThreadContext,
  clearPreview: typeof actions.clearPreview,
  selectSourceURL: typeof actions.selectSourceURL,
  exception: Exception,
  mouseout: Function,
};

type OwnProps = {|
  exception: Exception,
  mouseout: Function,
|};

type State = {
  isStacktraceExpanded: boolean,
};

const POPUP_SELECTOR = ".preview-popup.exception-popup";
const ANONYMOUS_FN_NAME = "<anonymous>";

// The exception popup works in two modes:
// a. when the stacktrace is closed the exception popup
// gets closed when the mouse leaves the popup.
// b. when the stacktrace is opened the exception popup
// gets closed only by clicking outside the popup.
class ExceptionPopup extends Component<Props, State> {
  topWindow: Object;

  constructor(props: Props) {
    super(props);
    this.state = {
      isStacktraceExpanded: false,
    };
  }

  updateTopWindow() {
    // The ChromeWindow is used when the stacktrace is expanded to capture all clicks
    // outside the popup so the popup can be closed only by clicking outside of it.
    if (this.topWindow) {
      this.topWindow.removeEventListener(
        "mousedown",
        this.onTopWindowClick,
        true
      );
      this.topWindow = null;
    }
    this.topWindow = DevToolsUtils.getTopWindow(window.parent);
    this.topWindow.addEventListener("mousedown", this.onTopWindowClick, true);
  }

  onTopWindowClick = (e: Object) => {
    const { cx, clearPreview } = this.props;

    // When the stactrace is expaned the exception popup gets closed
    // only by clicking ouside the popup.
    if (!e.target.closest(POPUP_SELECTOR)) {
      clearPreview(cx);
    }
  };

  onExceptionMessageClick() {
    const isStacktraceExpanded = this.state.isStacktraceExpanded;

    this.updateTopWindow();
    this.setState({ isStacktraceExpanded: !isStacktraceExpanded });
  }

  buildStackFrame(frame: StacktraceFrame) {
    const { cx, selectSourceURL } = this.props;
    const { filename, lineNumber } = frame;
    const functionName = frame.functionName || ANONYMOUS_FN_NAME;

    return (
      <div
        className="frame"
        onClick={() => selectSourceURL(cx, filename, { line: lineNumber })}
      >
        <span className="title">{functionName}</span>
        <span className="location">
          <span className="filename">{filename}</span>:
          <span className="line">{lineNumber}</span>
        </span>
      </div>
    );
  }

  renderStacktrace(stacktrace: StacktraceFrame[]) {
    const isStacktraceExpanded = this.state.isStacktraceExpanded;

    if (stacktrace.length && isStacktraceExpanded) {
      return (
        <div className="exception-stacktrace">
          {stacktrace.map(frame => this.buildStackFrame(frame))}
        </div>
      );
    }
    return null;
  }

  renderArrowIcon(stacktrace: StacktraceFrame[]) {
    if (stacktrace.length) {
      return (
        <AccessibleImage
          className={classnames("arrow", {
            expanded: this.state.isStacktraceExpanded,
          })}
        />
      );
    }
    return null;
  }

  render() {
    const {
      exception: { stacktrace, errorMessage },
      mouseout,
    } = this.props;

    return (
      <div
        className="preview-popup exception-popup"
        dir="ltr"
        onMouseLeave={() => mouseout(true, this.state.isStacktraceExpanded)}
      >
        <div
          className="exception-message"
          onClick={() => this.onExceptionMessageClick()}
        >
          {this.renderArrowIcon(stacktrace)}
          {StringRep.rep({
            object: errorMessage,
            useQuotes: false,
            className: "exception-text",
          })}
        </div>
        {this.renderStacktrace(stacktrace)}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  cx: getThreadContext(state),
});

const mapDispatchToProps = {
  selectSourceURL: actions.selectSourceURL,
  clearPreview: actions.clearPreview,
};

export default connect<Props, OwnProps, _, _, _, _>(
  mapStateToProps,
  mapDispatchToProps
)(ExceptionPopup);
