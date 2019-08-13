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
} from "../../selectors";

import type { Frame } from "../../types";

type Props = {
  editor: Object,
  selectedFrame: Frame,
  selectedSource: Object,
  previews: Object,
};

class InlinePreviews extends Component<Props> {
  render() {
    const { editor, selectedFrame, selectedSource, previews } = this.props;

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
        {Object.keys(previews).map(line => {
          return (
            <InlinePreviewRow
              editor={editor}
              line={parseInt(line, 10)}
              previews={previews[line]}
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
  };
};

export default connect(mapStateToProps)(InlinePreviews);
