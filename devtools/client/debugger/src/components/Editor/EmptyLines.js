/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { connect } from "../../utils/connect";
import { Component } from "react";
import { getSelectedSource, getSelectedBreakableLines } from "../../selectors";
import type { Source } from "../../types";
import { fromEditorLine } from "../../utils/editor";

type Props = {
  selectedSource: Source,
  editor: Object,
  breakableLines: Set<number>,
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
    const { breakableLines, selectedSource, editor } = this.props;

    editor.codeMirror.operation(() => {
      editor.codeMirror.eachLine(lineHandle => {
        const line = fromEditorLine(
          selectedSource.id,
          editor.codeMirror.getLineNumber(lineHandle)
        );

        if (breakableLines.has(line)) {
          editor.codeMirror.removeLineClass(lineHandle, "line", "empty-line");
        } else {
          editor.codeMirror.addLineClass(lineHandle, "line", "empty-line");
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
  const breakableLines = getSelectedBreakableLines(state);

  return {
    selectedSource,
    breakableLines,
  };
};

export default connect(mapStateToProps)(EmptyLines);
