/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import classnames from "classnames";
import { connect } from "react-redux";

import ExceptionOption from "./ExceptionOption";

import Breakpoint from "./Breakpoint";
import BreakpointHeading from "./BreakpointHeading";

import actions from "../../../actions";
import { getDisplayPath } from "../../../utils/source";
import { makeLocationId, sortBreakpoints } from "../../../utils/breakpoint";

import { getSelectedSource, getBreakpointSources } from "../../../selectors";

import type { Source } from "../../../types";
import type { BreakpointSources } from "../../../selectors/breakpointSources";

import "./Breakpoints.css";

type Props = {
  breakpointSources: BreakpointSources,
  selectedSource: Source,
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean,
  pauseOnExceptions: Function
};

class Breakpoints extends Component<Props> {
  renderExceptionsOptions() {
    const {
      breakpointSources,
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions
    } = this.props;

    const isEmpty = breakpointSources.length == 0;

    return (
      <div
        className={classnames("breakpoints-exceptions-options", {
          empty: isEmpty
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
    const { breakpointSources } = this.props;
    const sources = [
      ...breakpointSources.map(({ source, breakpoints }) => source)
    ];

    return [
      ...breakpointSources.map(({ source, breakpoints, i }) => {
        const path = getDisplayPath(source, sources);
        const sortedBreakpoints = sortBreakpoints(breakpoints);

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
              key={makeLocationId(breakpoint.selectedLocation)}
            />
          ))
        ];
      })
    ];
  }

  render() {
    return (
      <div className="pane breakpoints-list">
        {this.renderExceptionsOptions()}
        {this.renderBreakpoints()}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  breakpointSources: getBreakpointSources(state),
  selectedSource: getSelectedSource(state)
});

export default connect(
  mapStateToProps,
  {
    pauseOnExceptions: actions.pauseOnExceptions
  }
)(Breakpoints);
