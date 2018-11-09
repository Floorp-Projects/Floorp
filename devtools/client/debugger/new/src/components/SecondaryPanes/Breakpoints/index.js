/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import classnames from "classnames";
import { connect } from "react-redux";

import ExceptionOption from "./ExceptionOption";

import Breakpoint from "./Breakpoint";
import SourceIcon from "../../shared/SourceIcon";

import actions from "../../../actions";
import {
  getTruncatedFileName,
  getDisplayPath,
  getRawSourceURL
} from "../../../utils/source";
import { makeLocationId } from "../../../utils/breakpoint";

import { getSelectedSource, getBreakpointSources } from "../../../selectors";

import type { Source } from "../../../types";
import type { BreakpointSources } from "../../../selectors/breakpointSources";

import "./Breakpoints.css";

type Props = {
  breakpointSources: BreakpointSources,
  selectedSource: Source,
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean,
  pauseOnExceptions: Function,
  selectSource: string => void
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
        return [
          <div
            className="breakpoint-heading"
            title={getRawSourceURL(source.url)}
            key={source.url}
            onClick={() => this.props.selectSource(source.id)}
          >
            <SourceIcon
              source={source}
              shouldHide={icon => ["file", "javascript"].includes(icon)}
            />
            <div className="filename">
              {getTruncatedFileName(source)}
              {path && <span>{`../${path}/..`}</span>}
            </div>
          </div>,
          ...breakpoints.map(breakpoint => (
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
    pauseOnExceptions: actions.pauseOnExceptions,
    selectSource: actions.selectSource
  }
)(Breakpoints);
