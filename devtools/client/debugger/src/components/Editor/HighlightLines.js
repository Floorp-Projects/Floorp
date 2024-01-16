/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

class HighlightLines extends Component {
  static get propTypes() {
    return {
      editor: PropTypes.object.isRequired,
      range: PropTypes.object.isRequired,
    };
  }

  componentDidMount() {
    this.highlightLineRange();
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillUpdate() {
    this.clearHighlightRange();
  }

  componentDidUpdate() {
    this.highlightLineRange();
  }

  componentWillUnmount() {
    this.clearHighlightRange();
  }

  clearHighlightRange() {
    const { range, editor } = this.props;

    const { codeMirror } = editor;

    if (!range || !codeMirror) {
      return;
    }

    const { start, end } = range;
    codeMirror.operation(() => {
      for (let line = start - 1; line < end; line++) {
        codeMirror.removeLineClass(line, "wrap", "highlight-lines");
      }
    });
  }

  highlightLineRange = () => {
    const { range, editor } = this.props;

    const { codeMirror } = editor;

    if (!range || !codeMirror) {
      return;
    }

    const { start, end } = range;

    codeMirror.operation(() => {
      editor.alignLine(start);
      for (let line = start - 1; line < end; line++) {
        codeMirror.addLineClass(line, "wrap", "highlight-lines");
      }
    });
  };

  render() {
    return null;
  }
}

export default HighlightLines;
