/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import classnames from "classnames";
import { connect } from "../../../utils/connect";

import ExceptionOption from "./ExceptionOption";

import Breakpoint from "./Breakpoint";
import BreakpointHeading from "./BreakpointHeading";

import actions from "../../../actions";
import { getSelectedLocation } from "../../../utils/selected-location";
import { createHeadlessEditor } from "../../../utils/editor/create-editor";

import { makeBreakpointId } from "../../../utils/breakpoint";

import { getSelectedSource, getBreakpointSources } from "../../../selectors";

import "./Breakpoints.css";

class Breakpoints extends Component {
  static get propTypes() {
    return {
      breakpointSources: PropTypes.array.isRequired,
      pauseOnExceptions: PropTypes.func.isRequired,
      selectedSource: PropTypes.object,
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

  renderExceptionsOptions() {
    const {
      breakpointSources,
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions,
    } = this.props;

    const isEmpty = !breakpointSources.length;

    return (
      <div
        className={classnames("breakpoints-exceptions-options", {
          empty: isEmpty,
        })}
      >
        <ExceptionOption
          className="breakpoints-exceptions"
          label={L10N.getStr("pauseOnExceptionsItem2")}
          isChecked={shouldPauseOnExceptions}
          onChange={() => pauseOnExceptions(!shouldPauseOnExceptions, false)}
        />

        {shouldPauseOnExceptions && (
          <ExceptionOption
            className="breakpoints-exceptions-caught"
            label={L10N.getStr("pauseOnCaughtExceptionsItem")}
            isChecked={shouldPauseOnCaughtExceptions}
            onChange={() =>
              pauseOnExceptions(true, !shouldPauseOnCaughtExceptions)
            }
          />
        )}
      </div>
    );
  }

  renderBreakpoints() {
    const { breakpointSources, selectedSource } = this.props;
    if (!breakpointSources.length) {
      return null;
    }

    const editor = this.getEditor();
    const sources = breakpointSources.map(({ source }) => source);

    return (
      <div className="pane breakpoints-list">
        {breakpointSources.map(({ source, breakpoints }) => {
          return [
            <BreakpointHeading
              key={source.id}
              source={source}
              sources={sources}
            />,
            breakpoints.map(breakpoint => (
              <Breakpoint
                breakpoint={breakpoint}
                source={source}
                selectedSource={selectedSource}
                editor={editor}
                key={makeBreakpointId(
                  getSelectedLocation(breakpoint, selectedSource)
                )}
              />
            )),
          ];
        })}
      </div>
    );
  }

  render() {
    return (
      <div className="pane">
        {this.renderExceptionsOptions()}
        {this.renderBreakpoints()}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  breakpointSources: getBreakpointSources(state),
  selectedSource: getSelectedSource(state),
});

export default connect(mapStateToProps, {
  pauseOnExceptions: actions.pauseOnExceptions,
})(Breakpoints);
