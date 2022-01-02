/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import InlinePreviewRow from "./InlinePreviewRow";
import { connect } from "../../utils/connect";
import {
  getSelectedFrame,
  getCurrentThread,
  getInlinePreviews,
} from "../../selectors";

function hasPreviews(previews) {
  return !!previews && Object.keys(previews).length > 0;
}

class InlinePreviews extends Component {
  shouldComponentUpdate({ previews }) {
    return hasPreviews(previews);
  }

  render() {
    const { editor, selectedFrame, selectedSource, previews } = this.props;

    // Render only if currently open file is the one where debugger is paused
    if (
      !selectedFrame ||
      selectedFrame.location.sourceId !== selectedSource.id ||
      !hasPreviews(previews)
    ) {
      return null;
    }
    const previewsObj = previews;

    let inlinePreviewRows;
    editor.codeMirror.operation(() => {
      inlinePreviewRows = Object.keys(previewsObj).map(line => {
        const lineNum = parseInt(line, 10);

        return (
          <InlinePreviewRow
            editor={editor}
            key={line}
            line={lineNum}
            previews={previewsObj[line]}
          />
        );
      });
    });

    return <div>{inlinePreviewRows}</div>;
  }
}

const mapStateToProps = state => {
  const thread = getCurrentThread(state);
  const selectedFrame = getSelectedFrame(state, thread);

  if (!selectedFrame) {
    return {
      selectedFrame: null,
      previews: null,
    };
  }

  return {
    selectedFrame,
    previews: getInlinePreviews(state, thread, selectedFrame.id),
  };
};

export default connect(mapStateToProps)(InlinePreviews);
