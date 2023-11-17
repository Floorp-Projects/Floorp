/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { div } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../../utils/connect";

import ExceptionOption from "./ExceptionOption";

import Breakpoint from "./Breakpoint";
import BreakpointHeading from "./BreakpointHeading";

import actions from "../../../actions";
import { getSelectedLocation } from "../../../utils/selected-location";
import { createHeadlessEditor } from "../../../utils/editor/create-editor";

import { makeBreakpointId } from "../../../utils/breakpoint";

import {
  getSelectedSource,
  getBreakpointSources,
  getShouldPauseOnDebuggerStatement,
  getShouldPauseOnExceptions,
  getShouldPauseOnCaughtExceptions,
  supportsDebuggerStatementIgnore,
} from "../../../selectors";

const classnames = require("devtools/client/shared/classnames.js");

import "./Breakpoints.css";

class Breakpoints extends Component {
  static get propTypes() {
    return {
      breakpointSources: PropTypes.array.isRequired,
      pauseOnExceptions: PropTypes.func.isRequired,
      selectedSource: PropTypes.object,
      shouldPauseOnDebuggerStatement: PropTypes.bool.isRequired,
      shouldPauseOnCaughtExceptions: PropTypes.bool.isRequired,
      shouldPauseOnExceptions: PropTypes.bool.isRequired,
    };
  }

  componentWillUnmount() {
    this.removeEditor();
  }

  getEditor() {
    if (!this.headlessEditor) {
      this.headlessEditor = createHeadlessEditor();
    }
    return this.headlessEditor;
  }

  removeEditor() {
    if (!this.headlessEditor) {
      return;
    }
    this.headlessEditor.destroy();
    this.headlessEditor = null;
  }

  togglePauseOnDebuggerStatement = () => {
    this.props.pauseOnDebuggerStatement(
      !this.props.shouldPauseOnDebuggerStatement
    );
  };

  togglePauseOnException = () => {
    this.props.pauseOnExceptions(!this.props.shouldPauseOnExceptions, false);
  };

  togglePauseOnCaughtException = () => {
    this.props.pauseOnExceptions(
      true,
      !this.props.shouldPauseOnCaughtExceptions
    );
  };

  renderExceptionsOptions() {
    const {
      breakpointSources,
      shouldPauseOnDebuggerStatement,
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
    } = this.props;

    const isEmpty = !breakpointSources.length;
    return div(
      {
        className: classnames("breakpoints-options", {
          empty: isEmpty,
        }),
      },
      this.props.supportsDebuggerStatementIgnore
        ? React.createElement(ExceptionOption, {
            className: "breakpoints-debugger-statement",
            label: L10N.getStr("pauseOnDebuggerStatement"),
            isChecked: shouldPauseOnDebuggerStatement,
            onChange: this.togglePauseOnDebuggerStatement,
          })
        : null,
      React.createElement(ExceptionOption, {
        className: "breakpoints-exceptions",
        label: L10N.getStr("pauseOnExceptionsItem2"),
        isChecked: shouldPauseOnExceptions,
        onChange: this.togglePauseOnException,
      }),
      shouldPauseOnExceptions &&
        React.createElement(ExceptionOption, {
          className: "breakpoints-exceptions-caught",
          label: L10N.getStr("pauseOnCaughtExceptionsItem"),
          isChecked: shouldPauseOnCaughtExceptions,
          onChange: this.togglePauseOnCaughtException,
        })
    );
  }

  renderBreakpoints() {
    const { breakpointSources, selectedSource } = this.props;
    if (!breakpointSources.length) {
      return null;
    }

    const editor = this.getEditor();
    const sources = breakpointSources.map(({ source }) => source);
    return div(
      {
        className: "pane breakpoints-list",
      },
      breakpointSources.map(({ source, breakpoints }) => {
        return [
          React.createElement(BreakpointHeading, {
            key: source.id,
            source,
            sources,
          }),
          breakpoints.map(breakpoint =>
            React.createElement(Breakpoint, {
              breakpoint,
              source,
              editor,
              key: makeBreakpointId(
                getSelectedLocation(breakpoint, selectedSource)
              ),
            })
          ),
        ];
      })
    );
  }

  render() {
    return div(
      {
        className: "pane",
      },
      this.renderExceptionsOptions(),
      this.renderBreakpoints()
    );
  }
}

const mapStateToProps = state => ({
  breakpointSources: getBreakpointSources(state),
  selectedSource: getSelectedSource(state),
  shouldPauseOnDebuggerStatement: getShouldPauseOnDebuggerStatement(state),
  shouldPauseOnExceptions: getShouldPauseOnExceptions(state),
  shouldPauseOnCaughtExceptions: getShouldPauseOnCaughtExceptions(state),
  supportsDebuggerStatementIgnore: supportsDebuggerStatementIgnore(state),
});

export default connect(mapStateToProps, {
  pauseOnDebuggerStatement: actions.pauseOnDebuggerStatement,
  pauseOnExceptions: actions.pauseOnExceptions,
})(Breakpoints);
