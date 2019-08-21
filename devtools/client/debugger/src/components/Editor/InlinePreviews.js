/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import InlinePreviewRow from "./InlinePreviewRow";
import { connect } from "../../utils/connect";
import {
  getSelectedFrame,
  getCurrentThread,
  getInlinePreviews,
  visibleColumnBreakpoints,
} from "../../selectors";

import type { Frame } from "../../types";
import type { ColumnBreakpoint as ColumnBreakpointType } from "../../selectors/visibleColumnBreakpoints";

type Props = {
  editor: Object,
  selectedFrame: Frame,
  selectedSource: Object,
  previews: Object,
  columnBreakpoints: ColumnBreakpointType[],
};

class InlinePreviews extends Component<Props> {
  render() {
    const {
      editor,
      selectedFrame,
      selectedSource,
      previews,
      columnBreakpoints,
    } = this.props;

    // Render only if currently open file is the one where debugger is paused
    if (
      !selectedFrame ||
      selectedFrame.location.sourceId !== selectedSource.id ||
      !previews
    ) {
      return null;
    }

    return (
      <div>
        {Object.keys(previews).map((line: string) => {
          const lineNum: number = parseInt(line, 10);
          const numColumnBreakpoints = columnBreakpoints.filter(
            bp => bp.location.line === lineNum + 1
          ).length;

          return (
            <InlinePreviewRow
              editor={editor}
              line={lineNum}
              previews={previews[line]}
              numColumnBreakpoints={numColumnBreakpoints}
            />
          );
        })}
      </div>
    );
  }
}

const mapStateToProps = state => {
  const thread = getCurrentThread(state);
  const selectedFrame = getSelectedFrame(state, thread);

  if (!selectedFrame) return {};

  return {
    selectedFrame,
    previews: getInlinePreviews(state, thread, selectedFrame.id),
    columnBreakpoints: visibleColumnBreakpoints(state),
  };
};

export default connect(mapStateToProps)(InlinePreviews);
