/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import Breakpoint from "./Breakpoint";

import { getSelectedSource, getFirstVisibleBreakpoints } from "../../selectors";
import { makeLocationId } from "../../utils/breakpoint";
import { isLoaded } from "../../utils/source";
import { connect } from "../../utils/connect";

import type { Breakpoint as BreakpointType, Source } from "../../types";

type Props = {
  selectedSource: Source,
  breakpoints: BreakpointType[],
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
        {breakpoints.map(bp => {
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
  // Retrieves only the first breakpoint per line so that the
  // breakpoint marker represents only the first breakpoint
  breakpoints: getFirstVisibleBreakpoints(state),
  selectedSource: getSelectedSource(state)
}))(Breakpoints);
