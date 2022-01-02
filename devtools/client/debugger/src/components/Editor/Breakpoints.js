/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import Breakpoint from "./Breakpoint";

import { getSelectedSource, getFirstVisibleBreakpoints } from "../../selectors";
import { makeBreakpointId } from "../../utils/breakpoint";
import { connect } from "../../utils/connect";
import { breakpointItemActions } from "./menus/breakpoints";
import { editorItemActions } from "./menus/editor";

class Breakpoints extends Component {
  render() {
    const {
      cx,
      breakpoints,
      selectedSource,
      editor,
      breakpointActions,
      editorActions,
    } = this.props;

    if (!selectedSource || !breakpoints || selectedSource.isBlackBoxed) {
      return null;
    }

    return (
      <div>
        {breakpoints.map(bp => {
          return (
            <Breakpoint
              cx={cx}
              key={makeBreakpointId(bp.location)}
              breakpoint={bp}
              selectedSource={selectedSource}
              editor={editor}
              breakpointActions={breakpointActions}
              editorActions={editorActions}
            />
          );
        })}
      </div>
    );
  }
}

export default connect(
  state => ({
    // Retrieves only the first breakpoint per line so that the
    // breakpoint marker represents only the first breakpoint
    breakpoints: getFirstVisibleBreakpoints(state),
    selectedSource: getSelectedSource(state),
  }),
  dispatch => ({
    breakpointActions: breakpointItemActions(dispatch),
    editorActions: editorItemActions(dispatch),
  })
)(Breakpoints);
