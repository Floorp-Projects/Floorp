/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import { div, input, span } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../../utils/connect";
import { createSelector } from "reselect";
import actions from "../../../actions";

import { CloseButton } from "../../shared/Button";

import { getSelectedText, makeBreakpointId } from "../../../utils/breakpoint";
import { getSelectedLocation } from "../../../utils/selected-location";
import { isLineBlackboxed } from "../../../utils/source";

import {
  getSelectedFrame,
  getSelectedSource,
  getCurrentThread,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
  getBlackBoxRanges,
} from "../../../selectors";

const classnames = require("devtools/client/shared/classnames.js");

class Breakpoint extends PureComponent {
  static get propTypes() {
    return {
      breakpoint: PropTypes.object.isRequired,
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
      isBreakpointLineBlackboxed: PropTypes.bool,
      showBreakpointContextMenu: PropTypes.func.isRequired,
    };
  }

  onContextMenu = event => {
    event.preventDefault();

    this.props.showBreakpointContextMenu(
      event,
      this.props.breakpoint,
      this.props.source
    );
  };

  get selectedLocation() {
    const { breakpoint, selectedSource } = this.props;
    return getSelectedLocation(breakpoint, selectedSource);
  }

  stopClicks = event => event.stopPropagation();

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
    const { selectSpecificLocation } = this.props;
    selectSpecificLocation(this.selectedLocation);
  };

  removeBreakpoint = event => {
    const { removeBreakpoint, breakpoint } = this.props;
    event.stopPropagation();
    removeBreakpoint(breakpoint);
  };

  handleBreakpointCheckbox = () => {
    const { breakpoint, enableBreakpoint, disableBreakpoint } = this.props;
    if (breakpoint.disabled) {
      enableBreakpoint(breakpoint);
    } else {
      disableBreakpoint(breakpoint);
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
    // column is 0-based everywhere, but we want to display 1-based to the user.
    const columnVal = column ? `:${column + 1}` : "";
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
    const { breakpoint, editor, isBreakpointLineBlackboxed } = this.props;
    const text = this.getBreakpointText();
    const labelId = `${breakpoint.id}-label`;
    return div(
      {
        className: classnames({
          breakpoint,
          paused: this.isCurrentlyPausedAtBreakpoint(),
          disabled: breakpoint.disabled,
          "is-conditional": !!breakpoint.options.condition,
          "is-log": !!breakpoint.options.logValue,
        }),
        onClick: this.selectBreakpoint,
        onDoubleClick: this.onDoubleClick,
        onContextMenu: this.onContextMenu,
      },
      input({
        id: breakpoint.id,
        type: "checkbox",
        className: "breakpoint-checkbox",
        checked: !breakpoint.disabled,
        disabled: isBreakpointLineBlackboxed,
        onChange: this.handleBreakpointCheckbox,
        onClick: this.stopClicks,
        "aria-labelledby": labelId,
      }),
      span(
        {
          id: labelId,
          className: "breakpoint-label cm-s-mozilla devtools-monospace",
          onClick: this.selectBreakpoint,
          title: text,
        },
        span({
          dangerouslySetInnerHTML: this.highlightText(text, editor),
        })
      ),
      div(
        {
          className: "breakpoint-line-close",
        },
        div(
          {
            className: "breakpoint-line devtools-monospace",
          },
          this.getBreakpointLocation()
        ),
        React.createElement(CloseButton, {
          handleClick: this.removeBreakpoint,
          tooltip: L10N.getStr("breakpoints.removeBreakpointTooltip"),
        })
      )
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

const mapStateToProps = (state, props) => {
  const blackboxedRangesForSource = getBlackBoxRanges(state)[props.source.url];
  const isSourceOnIgnoreList =
    isSourceMapIgnoreListEnabled(state) &&
    isSourceOnSourceMapIgnoreList(state, props.source);
  return {
    selectedSource: getSelectedSource(state),
    isBreakpointLineBlackboxed: isLineBlackboxed(
      blackboxedRangesForSource,
      props.breakpoint.location.line,
      isSourceOnIgnoreList
    ),
    frame: getFormattedFrame(state, getCurrentThread(state)),
  };
};

export default connect(mapStateToProps, {
  enableBreakpoint: actions.enableBreakpoint,
  removeBreakpoint: actions.removeBreakpoint,
  disableBreakpoint: actions.disableBreakpoint,
  selectSpecificLocation: actions.selectSpecificLocation,
  openConditionalPanel: actions.openConditionalPanel,
  showBreakpointContextMenu: actions.showBreakpointContextMenu,
})(Breakpoint);
