/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { connect } from "../../utils/connect";
import { Component } from "react";
import { getSelectedSource, getEmptyLines } from "../../selectors";
import type { Source } from "../../types";
import { fromEditorLine } from "../../utils/editor";

type Props = {
  selectedSource: Source,
  editor: Object,
  emptyLines: Set<number>
};

class EmptyLines extends Component<Props> {
  componentDidMount() {
    this.disableEmptyLines();
  }

  componentDidUpdate() {
    this.disableEmptyLines();
  }

  componentWillUnmount() {
    const { editor } = this.props;

    editor.codeMirror.operation(() => {
      editor.codeMirror.eachLine(lineHandle => {
        editor.codeMirror.removeLineClass(lineHandle, "line", "empty-line");
      });
    });
  }

  disableEmptyLines() {
    const { emptyLines, selectedSource, editor } = this.props;

    editor.codeMirror.operation(() => {
      editor.codeMirror.eachLine(lineHandle => {
        const line = fromEditorLine(
          selectedSource.id,
          editor.codeMirror.getLineNumber(lineHandle)
        );

        if (emptyLines.has(line)) {
          editor.codeMirror.addLineClass(lineHandle, "line", "empty-line");
        } else {
          editor.codeMirror.removeLineClass(lineHandle, "line", "empty-line");
        }
      });
    });
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  if (!selectedSource) {
    throw new Error("no selectedSource");
  }
  const emptyLines = new Set(getEmptyLines(state, selectedSource.id) || []);

  return {
    selectedSource,
    emptyLines
  };
};

export default connect(mapStateToProps)(EmptyLines);
