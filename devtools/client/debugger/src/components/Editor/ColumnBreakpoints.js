/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";

import ColumnBreakpoint from "./ColumnBreakpoint";

import {
  getSelectedSource,
  visibleColumnBreakpoints,
  getContext,
} from "../../selectors";
import { connect } from "../../utils/connect";
import { makeBreakpointId } from "../../utils/breakpoint";
import { breakpointItemActions } from "./menus/breakpoints";

// eslint-disable-next-line max-len

class ColumnBreakpoints extends Component {
  render() {
    const {
      cx,
      editor,
      columnBreakpoints,
      selectedSource,
      breakpointActions,
    } = this.props;

    if (
      !selectedSource ||
      selectedSource.isBlackBoxed ||
      columnBreakpoints.length === 0
    ) {
      return null;
    }

    let breakpoints;
    editor.codeMirror.operation(() => {
      breakpoints = columnBreakpoints.map(breakpoint => (
        <ColumnBreakpoint
          cx={cx}
          key={makeBreakpointId(breakpoint.location)}
          columnBreakpoint={breakpoint}
          editor={editor}
          source={selectedSource}
          breakpointActions={breakpointActions}
        />
      ));
    });
    return <div>{breakpoints}</div>;
  }
}

const mapStateToProps = state => ({
  cx: getContext(state),
  selectedSource: getSelectedSource(state),
  columnBreakpoints: visibleColumnBreakpoints(state),
});

export default connect(mapStateToProps, dispatch => ({
  breakpointActions: breakpointItemActions(dispatch),
}))(ColumnBreakpoints);
