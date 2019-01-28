/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import { connect } from "../../../utils/connect";
import { createSelector } from "reselect";
import classnames from "classnames";
import actions from "../../../actions";
import { memoize } from "lodash";

import showContextMenu from "./BreakpointsContextMenu";
import { CloseButton } from "../../shared/Button";

import {
  getLocationWithoutColumn,
  getSelectedText
} from "../../../utils/breakpoint";
import { getSelectedLocation } from "../../../utils/source-maps";
import { features } from "../../../utils/prefs";
import { getEditor } from "../../../utils/editor";

import type {
  Breakpoint as BreakpointType,
  Frame,
  Source,
  SourceLocation
} from "../../../types";

type FormattedFrame = Frame & {
  selectedLocation: SourceLocation
};

import {
  getBreakpointsList,
  getSelectedFrame,
  getSelectedSource
} from "../../../selectors";

type Props = {
  breakpoint: BreakpointType,
  breakpoints: BreakpointType[],
  selectedSource: Source,
  source: Source,
  frame: FormattedFrame,
  enableBreakpoint: typeof actions.enableBreakpoint,
  removeBreakpoint: typeof actions.removeBreakpoint,
  removeBreakpoints: typeof actions.removeBreakpoints,
  removeAllBreakpoints: typeof actions.removeAllBreakpoints,
  disableBreakpoint: typeof actions.disableBreakpoint,
  setBreakpointCondition: typeof actions.setBreakpointCondition,
  toggleAllBreakpoints: typeof actions.toggleAllBreakpoints,
  toggleBreakpoints: typeof actions.toggleBreakpoints,
  toggleDisabledBreakpoint: typeof actions.toggleDisabledBreakpoint,
  openConditionalPanel: typeof actions.openConditionalPanel,
  selectSpecificLocation: typeof actions.selectSpecificLocation
};

class Breakpoint extends PureComponent<Props> {
  onContextMenu = e => {
    showContextMenu({ ...this.props, contextMenuEvent: e });
  };

  get selectedLocation() {
    const { breakpoint, selectedSource } = this.props;
    return getSelectedLocation(breakpoint, selectedSource);
  }

  onDoubleClick = () => {
    const { breakpoint, openConditionalPanel } = this.props;
    if (breakpoint.condition) {
      openConditionalPanel(this.selectedLocation);
    }
  };

  selectBreakpoint = event => {
    event.preventDefault();
    const { selectSpecificLocation } = this.props;
    selectSpecificLocation(this.selectedLocation);
  };

  removeBreakpoint = event => {
    const { removeBreakpoint } = this.props;
    event.stopPropagation();
    removeBreakpoint(this.selectedLocation);
  };

  handleBreakpointCheckbox = () => {
    const { breakpoint, enableBreakpoint, disableBreakpoint } = this.props;
    if (breakpoint.disabled) {
      enableBreakpoint(this.selectedLocation);
    } else {
      disableBreakpoint(this.selectedLocation);
    }
  };

  isCurrentlyPausedAtBreakpoint() {
    const { frame } = this.props;
    if (!frame) {
      return false;
    }

    const bpId = getLocationWithoutColumn(this.selectedLocation);
    const frameId = getLocationWithoutColumn(frame.selectedLocation);
    return bpId == frameId;
  }

  getBreakpointLocation() {
    const { source } = this.props;
    const { column, line } = this.selectedLocation;

    const isWasm = source && source.isWasm;
    const columnVal = features.columnBreakpoints && column ? `:${column}` : "";
    const bpLocation = isWasm
      ? `0x${line.toString(16).toUpperCase()}`
      : `${line}${columnVal}`;

    return bpLocation;
  }

  getBreakpointText() {
    const { breakpoint, selectedSource } = this.props;
    return breakpoint.condition || getSelectedText(breakpoint, selectedSource);
  }

  highlightText = memoize(
    (text = "", editor) => {
      if (!editor.CodeMirror) {
        return { __html: text };
      }

      const node = document.createElement("div");
      editor.CodeMirror.runMode(text, "application/javascript", node);
      return { __html: node.innerHTML };
    },
    (text, editor) => `${text} - ${editor.CodeMirror ? "editor" : ""}`
  );

  /* eslint-disable react/no-danger */
  render() {
    const { breakpoint } = this.props;
    const text = this.getBreakpointText();
    const editor = getEditor();

    return (
      <div
        className={classnames({
          breakpoint,
          paused: this.isCurrentlyPausedAtBreakpoint(),
          disabled: breakpoint.disabled,
          "is-conditional": !!breakpoint.condition,
          log: breakpoint.log
        })}
        onClick={this.selectBreakpoint}
        onDoubleClick={this.onDoubleClick}
        onContextMenu={this.onContextMenu}
      >
        <input
          id={breakpoint.id}
          type="checkbox"
          className="breakpoint-checkbox"
          checked={!breakpoint.disabled}
          onChange={this.handleBreakpointCheckbox}
          onClick={ev => ev.stopPropagation()}
        />
        <label
          htmlFor={breakpoint.id}
          className="breakpoint-label cm-s-mozilla"
          onClick={this.selectBreakpoint}
          title={text}
        >
          <span dangerouslySetInnerHTML={this.highlightText(text, editor)} />
        </label>
        <div className="breakpoint-line-close">
          <div className="breakpoint-line">{this.getBreakpointLocation()}</div>
          <CloseButton
            handleClick={e => this.removeBreakpoint(e)}
            tooltip={L10N.getStr("breakpoints.removeBreakpointTooltip")}
          />
        </div>
      </div>
    );
  }
}

const getFormattedFrame = createSelector(
  getSelectedSource,
  getSelectedFrame,
  (selectedSource: ?Source, frame: ?Frame): ?FormattedFrame => {
    if (!frame) {
      return null;
    }

    return {
      ...frame,
      selectedLocation: getSelectedLocation(frame, selectedSource)
    };
  }
);

const mapStateToProps = state => ({
  breakpoints: getBreakpointsList(state),
  frame: getFormattedFrame(state)
});

export default connect(
  mapStateToProps,
  {
    enableBreakpoint: actions.enableBreakpoint,
    removeBreakpoint: actions.removeBreakpoint,
    removeBreakpoints: actions.removeBreakpoints,
    removeAllBreakpoints: actions.removeAllBreakpoints,
    disableBreakpoint: actions.disableBreakpoint,
    selectSpecificLocation: actions.selectSpecificLocation,
    setBreakpointCondition: actions.setBreakpointCondition,
    toggleAllBreakpoints: actions.toggleAllBreakpoints,
    toggleBreakpoints: actions.toggleBreakpoints,
    toggleDisabledBreakpoint: actions.toggleDisabledBreakpoint,
    openConditionalPanel: actions.openConditionalPanel
  }
)(Breakpoint);
