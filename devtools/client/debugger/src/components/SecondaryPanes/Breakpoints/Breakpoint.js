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
  getSelectedText,
  makeBreakpointId,
} from "../../../utils/breakpoint";
import { getSelectedLocation } from "../../../utils/selected-location";
import { features } from "../../../utils/prefs";

import type {
  Breakpoint as BreakpointType,
  Frame,
  Source,
  SourceLocation,
  Context,
} from "../../../types";
import type SourceEditor from "../../../utils/editor/source-editor";

type FormattedFrame = Frame & {
  selectedLocation: SourceLocation,
};

import {
  getBreakpointsList,
  getSelectedFrame,
  getSelectedSource,
  getCurrentThread,
  getContext,
} from "../../../selectors";

type Props = {
  cx: Context,
  breakpoint: BreakpointType,
  breakpoints: BreakpointType[],
  selectedSource: Source,
  source: Source,
  frame: FormattedFrame,
  editor: SourceEditor,
  enableBreakpoint: typeof actions.enableBreakpoint,
  removeBreakpoint: typeof actions.removeBreakpoint,
  removeBreakpoints: typeof actions.removeBreakpoints,
  removeAllBreakpoints: typeof actions.removeAllBreakpoints,
  disableBreakpoint: typeof actions.disableBreakpoint,
  setBreakpointOptions: typeof actions.setBreakpointOptions,
  toggleAllBreakpoints: typeof actions.toggleAllBreakpoints,
  toggleBreakpoints: typeof actions.toggleBreakpoints,
  toggleDisabledBreakpoint: typeof actions.toggleDisabledBreakpoint,
  openConditionalPanel: typeof actions.openConditionalPanel,
  selectSpecificLocation: typeof actions.selectSpecificLocation,
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
    if (breakpoint.options.condition) {
      openConditionalPanel(this.selectedLocation);
    } else if (breakpoint.options.logValue) {
      openConditionalPanel(this.selectedLocation, true);
    }
  };

  selectBreakpoint = event => {
    event.preventDefault();
    const { cx, selectSpecificLocation } = this.props;
    selectSpecificLocation(cx, this.selectedLocation);
  };

  removeBreakpoint = event => {
    const { cx, removeBreakpoint, breakpoint } = this.props;
    event.stopPropagation();
    removeBreakpoint(cx, breakpoint);
  };

  handleBreakpointCheckbox = () => {
    const { cx, breakpoint, enableBreakpoint, disableBreakpoint } = this.props;
    if (breakpoint.disabled) {
      enableBreakpoint(cx, breakpoint);
    } else {
      disableBreakpoint(cx, breakpoint);
    }
  };

  isCurrentlyPausedAtBreakpoint() {
    const { frame } = this.props;
    if (!frame) {
      return false;
    }

    const bpId = features.columnBreakpoints
      ? makeBreakpointId(this.selectedLocation)
      : getLocationWithoutColumn(this.selectedLocation);
    const frameId = features.columnBreakpoints
      ? makeBreakpointId(frame.selectedLocation)
      : getLocationWithoutColumn(frame.selectedLocation);
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
    const { condition, logValue } = breakpoint.options;
    return logValue || condition || getSelectedText(breakpoint, selectedSource);
  }

  highlightText = memoize(
    (text = "", editor) => {
      const node = document.createElement("div");
      editor.CodeMirror.runMode(text, "application/javascript", node);
      return { __html: node.innerHTML };
    },
    text => text
  );

  render() {
    const { breakpoint, editor } = this.props;
    const text = this.getBreakpointText();
    const labelId = `${breakpoint.id}-label`;

    return (
      <div
        className={classnames({
          breakpoint,
          paused: this.isCurrentlyPausedAtBreakpoint(),
          disabled: breakpoint.disabled,
          "is-conditional": !!breakpoint.options.condition,
          "is-log": !!breakpoint.options.logValue,
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
          aria-labelledby={labelId}
        />
        <span
          id={labelId}
          className="breakpoint-label cm-s-mozilla devtools-monospace"
          onClick={this.selectBreakpoint}
          title={text}
        >
          <span dangerouslySetInnerHTML={this.highlightText(text, editor)} />
        </span>
        <div className="breakpoint-line-close">
          <div className="breakpoint-line devtools-monospace">
            {this.getBreakpointLocation()}
          </div>
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
      selectedLocation: getSelectedLocation(frame, selectedSource),
    };
  }
);

const mapStateToProps = state => ({
  cx: getContext(state),
  breakpoints: getBreakpointsList(state),
  frame: getFormattedFrame(state, getCurrentThread(state)),
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
    setBreakpointOptions: actions.setBreakpointOptions,
    toggleAllBreakpoints: actions.toggleAllBreakpoints,
    toggleBreakpoints: actions.toggleBreakpoints,
    toggleDisabledBreakpoint: actions.toggleDisabledBreakpoint,
    openConditionalPanel: actions.openConditionalPanel,
  }
)(Breakpoint);
