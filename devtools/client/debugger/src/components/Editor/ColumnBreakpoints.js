/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { div } from "react-dom-factories";
import PropTypes from "prop-types";

import ColumnBreakpoint from "./ColumnBreakpoint";

import {
  getSelectedSource,
  visibleColumnBreakpoints,
  isSourceBlackBoxed,
} from "../../selectors";
import actions from "../../actions";
import { connect } from "../../utils/connect";
import { makeBreakpointId } from "../../utils/breakpoint";

// eslint-disable-next-line max-len

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
