/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React, { Component } from "react";
import Breakpoint from "./Breakpoint";

import {
  getSelectedSource,
  getFirstVisibleBreakpoints,
  getBlackBoxRanges,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
} from "../../selectors";
import { makeBreakpointId } from "../../utils/breakpoint";
import { connect } from "../../utils/connect";
import { breakpointItemActions } from "./menus/breakpoints";
import { editorItemActions } from "./menus/editor";

class Breakpoints extends Component {
  static get propTypes() {
    return {
      cx: PropTypes.object,
      breakpoints: PropTypes.array,
      editor: PropTypes.object,
      breakpointActions: PropTypes.object,
      editorActions: PropTypes.object,
      selectedSource: PropTypes.object,
      blackboxedRanges: PropTypes.object,
      isSelectedSourceOnIgnoreList: PropTypes.bool,
      blackboxedRangesForSelectedSource: PropTypes.array,
    };
  }
  render() {
    const {
      cx,
      breakpoints,
      selectedSource,
      editor,
      breakpointActions,
      editorActions,
      blackboxedRangesForSelectedSource,
      isSelectedSourceOnIgnoreList,
    } = this.props;

    if (!selectedSource || !breakpoints) {
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
              blackboxedRangesForSelectedSource={
                blackboxedRangesForSelectedSource
              }
              isSelectedSourceOnIgnoreList={isSelectedSourceOnIgnoreList}
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
  state => {
    const selectedSource = getSelectedSource(state);
    const blackboxedRanges = getBlackBoxRanges(state);
    return {
      // Retrieves only the first breakpoint per line so that the
      // breakpoint marker represents only the first breakpoint
      breakpoints: getFirstVisibleBreakpoints(state),
      selectedSource,
      blackboxedRangesForSelectedSource:
        selectedSource && blackboxedRanges[selectedSource.url],
      isSelectedSourceOnIgnoreList:
        selectedSource &&
        isSourceMapIgnoreListEnabled(state) &&
        isSourceOnSourceMapIgnoreList(state, selectedSource),
    };
  },
  dispatch => ({
    breakpointActions: breakpointItemActions(dispatch),
    editorActions: editorItemActions(dispatch),
  })
)(Breakpoints);
