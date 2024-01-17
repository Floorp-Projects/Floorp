/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import InlinePreviewRow from "./InlinePreviewRow";
import { connect } from "devtools/client/shared/vendor/react-redux";
import {
  getSelectedFrame,
  getCurrentThread,
  getInlinePreviews,
} from "../../selectors/index";

function hasPreviews(previews) {
  return !!previews && !!Object.keys(previews).length;
}

class InlinePreviews extends Component {
  static get propTypes() {
    return {
      editor: PropTypes.object.isRequired,
      previews: PropTypes.object,
      selectedFrame: PropTypes.object.isRequired,
      selectedSource: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate({ previews }) {
    return hasPreviews(previews);
  }

  render() {
    const { editor, selectedFrame, selectedSource, previews } = this.props;

    // Render only if currently open file is the one where debugger is paused
    if (
      !selectedFrame ||
      selectedFrame.location.source.id !== selectedSource.id ||
      !hasPreviews(previews)
    ) {
      return null;
    }
    const previewsObj = previews;

    let inlinePreviewRows;
    editor.codeMirror.operation(() => {
      inlinePreviewRows = Object.keys(previewsObj).map(line => {
        const lineNum = parseInt(line, 10);
        return React.createElement(InlinePreviewRow, {
          editor: editor,
          key: line,
          line: lineNum,
          previews: previewsObj[line],
        });
      });
    });
    return div(null, inlinePreviewRows);
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
