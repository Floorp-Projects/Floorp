/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import { features } from "../../utils/prefs";
const classnames = require("resource://devtools/client/shared/classnames.js");

import ColumnBreakpoint from "./ColumnBreakpoint";

import {
  getSelectedSource,
  visibleColumnBreakpoints,
  isSourceBlackBoxed,
} from "../../selectors/index";
import actions from "../../actions/index";
import { connect } from "devtools/client/shared/vendor/react-redux";
import { makeBreakpointId } from "../../utils/breakpoint/index";
import { fromEditorLine } from "../../utils/editor/index";

const breakpointButton = document.createElement("button");
const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
svg.setAttribute("viewBox", "0 0 11 13");
svg.setAttribute("width", 11);
svg.setAttribute("height", 13);

const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
path.setAttributeNS(
  null,
  "d",
  "M5.07.5H1.5c-.54 0-1 .46-1 1v10c0 .54.46 1 1 1h3.57c.58 0 1.15-.26 1.53-.7l3.7-5.3-3.7-5.3C6.22.76 5.65.5 5.07.5z"
);

svg.appendChild(path);
breakpointButton.appendChild(svg);

const COLUMN_BREAKPOINT_MARKER = "column-breakpoint-marker";

class ColumnBreakpoints extends Component {
  static get propTypes() {
    return {
      columnBreakpoints: PropTypes.array.isRequired,
      editor: PropTypes.object.isRequired,
      selectedSource: PropTypes.object,
      addBreakpoint: PropTypes.func,
      removeBreakpoint: PropTypes.func,
      toggleDisabledBreakpoint: PropTypes.func,
      showEditorCreateBreakpointContextMenu: PropTypes.func,
      showEditorEditBreakpointContextMenu: PropTypes.func,
    };
  }

  componentDidUpdate() {
    const { selectedSource, columnBreakpoints, editor } = this.props;

    // Only for codemirror 6
    if (!features.codemirrorNext) {
      return;
    }

    if (!selectedSource || !editor) {
      return;
    }

    if (!columnBreakpoints.length) {
      editor.removePositionContentMarker(COLUMN_BREAKPOINT_MARKER);
      return;
    }

    editor.setPositionContentMarker({
      id: COLUMN_BREAKPOINT_MARKER,
      positions: columnBreakpoints.map(bp => bp.location),
      createPositionElementNode: (line, column) => {
        const lineNumber = fromEditorLine(selectedSource.id, line);
        const columnBreakpoint = columnBreakpoints.find(
          bp => bp.location.line === lineNumber && bp.location.column === column
        );
        const breakpointNode = breakpointButton.cloneNode(true);
        breakpointNode.className = classnames("column-breakpoint", {
          "has-condition": columnBreakpoint.breakpoint?.options.condition,
          "has-log": columnBreakpoint.breakpoint?.options.logValue,
          active:
            columnBreakpoint.breakpoint &&
            !columnBreakpoint.breakpoint.disabled,
          disabled: columnBreakpoint.breakpoint?.disabled,
        });
        breakpointNode.addEventListener("click", event =>
          this.onClick(event, columnBreakpoint)
        );
        breakpointNode.addEventListener("contextmenu", event =>
          this.onContextMenu(event, columnBreakpoint)
        );
        return breakpointNode;
      },
    });
  }

  onClick = (event, columnBreakpoint) => {
    event.stopPropagation();
    event.preventDefault();
    const { toggleDisabledBreakpoint, removeBreakpoint, addBreakpoint } =
      this.props;

    // disable column breakpoint on shift-click.
    if (event.shiftKey) {
      toggleDisabledBreakpoint(columnBreakpoint.breakpoint);
      return;
    }

    if (columnBreakpoint.breakpoint) {
      removeBreakpoint(columnBreakpoint.breakpoint);
    } else {
      addBreakpoint(columnBreakpoint.location);
    }
  };

  onContextMenu = (event, columnBreakpoint) => {
    event.stopPropagation();
    event.preventDefault();

    if (columnBreakpoint.breakpoint) {
      this.props.showEditorEditBreakpointContextMenu(
        event,
        columnBreakpoint.breakpoint
      );
    } else {
      this.props.showEditorCreateBreakpointContextMenu(
        event,
        columnBreakpoint.location
      );
    }
  };

  render() {
    const {
      editor,
      columnBreakpoints,
      selectedSource,
      showEditorCreateBreakpointContextMenu,
      showEditorEditBreakpointContextMenu,
      toggleDisabledBreakpoint,
      removeBreakpoint,
      addBreakpoint,
    } = this.props;

    if (features.codemirrorNext) {
      return null;
    }

    if (!selectedSource || columnBreakpoints.length === 0) {
      return null;
    }

    let breakpoints;
    editor.codeMirror.operation(() => {
      breakpoints = columnBreakpoints.map(columnBreakpoint =>
        React.createElement(ColumnBreakpoint, {
          key: makeBreakpointId(columnBreakpoint.location),
          columnBreakpoint,
          editor,
          source: selectedSource,
          showEditorCreateBreakpointContextMenu,
          showEditorEditBreakpointContextMenu,
          toggleDisabledBreakpoint,
          removeBreakpoint,
          addBreakpoint,
        })
      );
    });
    return div(null, breakpoints);
  }
}

const mapStateToProps = state => {
  // Avoid rendering this component is there is no selected source,
  // or if the selected source is blackboxed.
  // Also avoid computing visible column breakpoint when this happens.
  const selectedSource = getSelectedSource(state);
  if (!selectedSource || isSourceBlackBoxed(state, selectedSource)) {
    return {};
  }
  return {
    selectedSource,
    columnBreakpoints: visibleColumnBreakpoints(state),
  };
};

export default connect(mapStateToProps, {
  showEditorCreateBreakpointContextMenu:
    actions.showEditorCreateBreakpointContextMenu,
  showEditorEditBreakpointContextMenu:
    actions.showEditorEditBreakpointContextMenu,
  toggleDisabledBreakpoint: actions.toggleDisabledBreakpoint,
  removeBreakpoint: actions.removeBreakpoint,
  addBreakpoint: actions.addBreakpoint,
})(ColumnBreakpoints);
