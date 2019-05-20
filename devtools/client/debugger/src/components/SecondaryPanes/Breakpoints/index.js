/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import classnames from "classnames";
import { connect } from "../../../utils/connect";

import ExceptionOption from "./ExceptionOption";

import Breakpoint from "./Breakpoint";
import BreakpointHeading from "./BreakpointHeading";

import actions from "../../../actions";
import { getDisplayPath } from "../../../utils/source";
import { getSelectedLocation } from "../../../utils/selected-location";
import { createHeadlessEditor } from "../../../utils/editor/create-editor";

import {
  makeBreakpointId,
  sortSelectedBreakpoints,
} from "../../../utils/breakpoint";

import {
  getSelectedSource,
  getBreakpointSources,
  getSkipPausing,
} from "../../../selectors";

import type { Source } from "../../../types";
import type { BreakpointSources } from "../../../selectors/breakpointSources";
import type SourceEditor from "../../../utils/editor/source-editor";

import "./Breakpoints.css";

type Props = {
  breakpointSources: BreakpointSources,
  selectedSource: Source,
  skipPausing: boolean,
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean,
  pauseOnExceptions: Function,
};

class Breakpoints extends Component<Props> {
  headlessEditor: ?SourceEditor;

  componentWillUnmount() {
    this.removeEditor();
  }

  getEditor(): SourceEditor {
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
    this.headlessEditor = (null: any);
  }

  renderExceptionsOptions() {
    const {
      breakpointSources,
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions,
    } = this.props;

    const isEmpty = breakpointSources.length == 0;

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

    const sources = [
      ...breakpointSources.map(({ source, breakpoints }) => source),
    ];

    return (
      <div className="pane breakpoints-list">
        {breakpointSources.map(({ source, breakpoints, i }) => {
          const path = getDisplayPath(source, sources);
          const sortedBreakpoints = sortSelectedBreakpoints(
            breakpoints,
            selectedSource
          );

          return [
            <BreakpointHeading
              source={source}
              sources={sources}
              path={path}
              key={source.url}
            />,
            ...sortedBreakpoints.map(breakpoint => (
              <Breakpoint
                breakpoint={breakpoint}
                source={source}
                selectedSource={selectedSource}
                editor={this.getEditor()}
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
    const { skipPausing } = this.props;
    return (
      <div className={classnames("pane", skipPausing && "skip-pausing")}>
        {this.renderExceptionsOptions()}
        {this.renderBreakpoints()}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  breakpointSources: getBreakpointSources(state),
  selectedSource: getSelectedSource(state),
  skipPausing: getSkipPausing(state),
});

export default connect(
  mapStateToProps,
  {
    pauseOnExceptions: actions.pauseOnExceptions,
  }
)(Breakpoints);
