/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { div, span } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../../utils/connect";

import Reps from "devtools/client/shared/components/reps/index";
const {
  REPS: { StringRep },
} = Reps;

import actions from "../../../actions";

import AccessibleImage from "../../shared/AccessibleImage";
const classnames = require("devtools/client/shared/classnames.js");
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
      isStacktraceExpanded: true,
    };
  }

  static get propTypes() {
    return {
      mouseout: PropTypes.func.isRequired,
      selectSourceURL: PropTypes.func.isRequired,
      exception: PropTypes.object.isRequired,
    };
  }

  onExceptionMessageClick() {
    const isStacktraceExpanded = this.state.isStacktraceExpanded;
    this.setState({ isStacktraceExpanded: !isStacktraceExpanded });
  }

  buildStackFrame(frame) {
    const { filename, lineNumber } = frame;
    const functionName = frame.functionName || ANONYMOUS_FN_NAME;
    return div(
      {
        className: "frame",
        onClick: () =>
          this.props.selectSourceURL(filename, {
            line: lineNumber,
          }),
      },
      span(
        {
          className: "title",
        },
        functionName
      ),
      span(
        {
          className: "location",
        },
        span(
          {
            className: "filename",
          },
          filename
        ),
        ":",
        span(
          {
            className: "line",
          },
          lineNumber
        )
      )
    );
  }

  renderStacktrace(stacktrace) {
    const isStacktraceExpanded = this.state.isStacktraceExpanded;

    if (stacktrace.length && isStacktraceExpanded) {
      return div(
        {
          className: "exception-stacktrace",
        },
        stacktrace.map(frame => this.buildStackFrame(frame))
      );
    }
    return null;
  }

  renderArrowIcon(stacktrace) {
    if (stacktrace.length) {
      return React.createElement(AccessibleImage, {
        className: classnames("arrow", {
          expanded: this.state.isStacktraceExpanded,
        }),
      });
    }
    return null;
  }

  render() {
    const {
      exception: { stacktrace, errorMessage },
      mouseout,
    } = this.props;
    return div(
      {
        className: "preview-popup exception-popup",
        dir: "ltr",
        onMouseLeave: () => mouseout(true, this.state.isStacktraceExpanded),
      },
      div(
        {
          className: "exception-message",
          onClick: () => this.onExceptionMessageClick(),
        },
        this.renderArrowIcon(stacktrace),
        StringRep.rep({
          object: errorMessage,
          useQuotes: false,
          className: "exception-text",
        })
      ),
      this.renderStacktrace(stacktrace)
    );
  }
}

const mapDispatchToProps = {
  selectSourceURL: actions.selectSourceURL,
};

export default connect(null, mapDispatchToProps)(ExceptionPopup);
