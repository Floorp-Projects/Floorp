/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { connect } from "../../utils/connect";
import { getHighlightedLineRange } from "../../selectors";

class HighlightLines extends Component {
  componentDidMount() {
    this.highlightLineRange();
  }

  componentWillUpdate() {
    this.clearHighlightRange();
  }

  componentDidUpdate() {
    this.highlightLineRange();
  }

  componentWillUnmount() {
    this.clearHighlightRange();
  }

  clearHighlightRange() {
    const { highlightedLineRange, editor } = this.props;

    const { codeMirror } = editor;

    if (!highlightedLineRange || !codeMirror) {
      return;
    }

    const { start, end } = highlightedLineRange;
    codeMirror.operation(() => {
      for (let line = start - 1; line < end; line++) {
        codeMirror.removeLineClass(line, "wrapClass", "highlight-lines");
      }
    });
  }

  highlightLineRange = () => {
    const { highlightedLineRange, editor } = this.props;

    const { codeMirror } = editor;

    if (!highlightedLineRange || !codeMirror) {
      return;
    }

    const { start, end } = highlightedLineRange;

    codeMirror.operation(() => {
      editor.alignLine(start);
      for (let line = start - 1; line < end; line++) {
        codeMirror.addLineClass(line, "wrapClass", "highlight-lines");
      }
    });
  };

  render() {
    return null;
  }
}

export default connect(state => ({
  highlightedLineRange: getHighlightedLineRange(state),
}))(HighlightLines);
