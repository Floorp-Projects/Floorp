/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import PropTypes from "prop-types";
import { connect } from "../../../utils/connect";
import { createSelector } from "reselect";
import actions from "../../../actions";

import showContextMenu from "./BreakpointsContextMenu";
import { CloseButton } from "../../shared/Button";

import { getSelectedText, makeBreakpointId } from "../../../utils/breakpoint";
import { getSelectedLocation } from "../../../utils/selected-location";
import { isLineBlackboxed } from "../../../utils/source";

import {
  getBreakpointsList,
  getSelectedFrame,
  getSelectedSource,
  getCurrentThread,
  getContext,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
} from "../../../selectors";

const classnames = require("devtools/client/shared/classnames.js");

class Breakpoint extends PureComponent {
  static get propTypes() {
    return {
      breakpoint: PropTypes.object.isRequired,
      cx: PropTypes.object.isRequired,
      disableBreakpoint: PropTypes.func.isRequired,
      editor: PropTypes.object.isRequired,
      enableBreakpoint: PropTypes.func.isRequired,
      frame: PropTypes.object,
      openConditionalPanel: PropTypes.func.isRequired,
      removeBreakpoint: PropTypes.func.isRequired,
      selectSpecificLocation: PropTypes.func.isRequired,
      selectedSource: PropTypes.object,
      source: PropTypes.object.isRequired,
      blackboxedRangesForSource: PropTypes.array.isRequired,
      checkSourceOnIgnoreList: PropTypes.func.isRequired,
    };
  }

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

    const bpId = makeBreakpointId(this.selectedLocation);
    const frameId = makeBreakpointId(frame.selectedLocation);
    return bpId == frameId;
  }

  getBreakpointLocation() {
    const { source } = this.props;
    const { column, line } = this.selectedLocation;

    const isWasm = source?.isWasm;
    const columnVal = column ? `:${column}` : "";
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

  highlightText(text = "", editor) {
    const node = document.createElement("div");
    editor.CodeMirror.runMode(text, "application/javascript", node);
    return { __html: node.innerHTML };
  }

  render() {
    const {
      breakpoint,
      editor,
      blackboxedRangesForSource,
      checkSourceOnIgnoreList,
    } = this.props;
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
          disabled={isLineBlackboxed(
            blackboxedRangesForSource,
            breakpoint.location.line,
            checkSourceOnIgnoreList(breakpoint.location.source)
          )}
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
  (selectedSource, frame) => {
    if (!frame) {
      return null;
    }

    return {
      ...frame,
      selectedLocation: getSelectedLocation(frame, selectedSource),
    };
  }
);

const mapStateToProps = (state, p) => ({
  cx: getContext(state),
  breakpoints: getBreakpointsList(state),
  frame: getFormattedFrame(state, getCurrentThread(state)),
  checkSourceOnIgnoreList: source =>
    isSourceMapIgnoreListEnabled(state) &&
    isSourceOnSourceMapIgnoreList(state, source),
});

export default connect(mapStateToProps, {
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
})(Breakpoint);
