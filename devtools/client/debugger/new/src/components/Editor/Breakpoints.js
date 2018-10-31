/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { connect } from "react-redux";
import React, { Component } from "react";

import Breakpoint from "./Breakpoint";

import { getSelectedSource, getVisibleBreakpoints } from "../../selectors";
import { makeLocationId } from "../../utils/breakpoint";
import { isLoaded } from "../../utils/source";

import type { BreakpointsMap } from "../../reducers/types";
import type { Source } from "../../types";

type Props = {
  selectedSource: Source,
  breakpoints: BreakpointsMap,
  editor: Object
};

class Breakpoints extends Component<Props> {
  shouldComponentUpdate(nextProps: Props) {
    if (nextProps.selectedSource && !isLoaded(nextProps.selectedSource)) {
      return false;
    }

    return true;
  }

  render() {
    const { breakpoints, selectedSource, editor } = this.props;

    if (!selectedSource || !breakpoints || selectedSource.isBlackBoxed) {
      return null;
    }

    return (
      <div>
        {breakpoints.valueSeq().map(bp => {
          return (
            <Breakpoint
              key={makeLocationId(bp.location)}
              breakpoint={bp}
              selectedSource={selectedSource}
              editor={editor}
            />
          );
        })}
      </div>
    );
  }
}

export default connect(state => ({
  breakpoints: getVisibleBreakpoints(state),
  selectedSource: getSelectedSource(state)
}))(Breakpoints);
