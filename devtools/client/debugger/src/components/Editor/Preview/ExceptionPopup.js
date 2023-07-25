/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../../../utils/connect";

import Reps from "devtools/client/shared/components/reps/index";
const {
  REPS: { StringRep },
} = Reps;

import actions from "../../../actions";

import AccessibleImage from "../../shared/AccessibleImage";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const classnames = require("devtools/client/shared/classnames.js");

const POPUP_SELECTOR = ".preview-popup.exception-popup";
const ANONYMOUS_FN_NAME = "<anonymous>";

// The exception popup works in two modes:
// a. when the stacktrace is closed the exception popup
// gets closed when the mouse leaves the popup.
// b. when the stacktrace is opened the exception popup
// gets closed only by clicking outside the popup.
class ExceptionPopup extends Component {
  constructor(props) {
    super(props);
    this.state = {
      isStacktraceExpanded: false,
    };
  }

  static get propTypes() {
    return {
      clearPreview: PropTypes.func.isRequired,
      mouseout: PropTypes.func.isRequired,
      selectSourceURL: PropTypes.func.isRequired,
      exception: PropTypes.object.isRequired,
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

  onTopWindowClick = e => {
    // When the stactrace is expaned the exception popup gets closed
    // only by clicking ouside the popup.
    if (!e.target.closest(POPUP_SELECTOR)) {
      this.props.clearPreview();
    }
  };

  onExceptionMessageClick() {
    const isStacktraceExpanded = this.state.isStacktraceExpanded;

    this.updateTopWindow();
    this.setState({ isStacktraceExpanded: !isStacktraceExpanded });
  }

  buildStackFrame(frame) {
    const { filename, lineNumber } = frame;
    const functionName = frame.functionName || ANONYMOUS_FN_NAME;

    return (
      <div
        className="frame"
        onClick={() =>
          this.props.selectSourceURL(filename, { line: lineNumber })
        }
      >
        <span className="title">{functionName}</span>
        <span className="location">
          <span className="filename">{filename}</span>:
          <span className="line">{lineNumber}</span>
        </span>
      </div>
    );
  }

  renderStacktrace(stacktrace) {
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

  renderArrowIcon(stacktrace) {
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

const mapDispatchToProps = {
  selectSourceURL: actions.selectSourceURL,
};

export default connect(null, mapDispatchToProps)(ExceptionPopup);
