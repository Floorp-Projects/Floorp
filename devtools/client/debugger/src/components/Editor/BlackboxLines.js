/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { Component } from "devtools/client/shared/vendor/react";
import { toEditorLine, fromEditorLine } from "../../utils/editor/index";
import { isLineBlackboxed } from "../../utils/source";
import { isWasm } from "../../utils/wasm";

// This renders blackbox line highlighting in the editor
class BlackboxLines extends Component {
  static get propTypes() {
    return {
      editor: PropTypes.object.isRequired,
      selectedSource: PropTypes.object.isRequired,
      blackboxedRangesForSelectedSource: PropTypes.array,
      isSourceOnIgnoreList: PropTypes.bool,
    };
  }

  componentDidMount() {
    const { selectedSource, blackboxedRangesForSelectedSource, editor } =
      this.props;

    if (this.props.isSourceOnIgnoreList) {
      this.setAllBlackboxLines(editor);
      return;
    }

    // When `blackboxedRangesForSelectedSource` is defined and the array is empty,
    // the whole source was blackboxed.
    if (!blackboxedRangesForSelectedSource.length) {
      this.setAllBlackboxLines(editor);
    } else {
      editor.codeMirror.operation(() => {
        blackboxedRangesForSelectedSource.forEach(range => {
          const start = toEditorLine(selectedSource.id, range.start.line);
          // CodeMirror.eachLine doesn't include `end` line offset, so bump by one
          const end = toEditorLine(selectedSource.id, range.end.line) + 1;
          editor.codeMirror.eachLine(start, end, lineHandle => {
            this.setBlackboxLine(editor, lineHandle);
          });
        });
      });
    }
  }

  componentDidUpdate() {
    const {
      selectedSource,
      blackboxedRangesForSelectedSource,
      editor,
      isSourceOnIgnoreList,
    } = this.props;

    if (this.props.isSourceOnIgnoreList) {
      this.setAllBlackboxLines(editor);
      return;
    }

    // when unblackboxed
    if (!blackboxedRangesForSelectedSource) {
      this.clearAllBlackboxLines(editor);
      return;
    }

    // When the whole source is blackboxed
    if (!blackboxedRangesForSelectedSource.length) {
      this.setAllBlackboxLines(editor);
      return;
    }

    const sourceIsWasm = isWasm(selectedSource.id);

    // TODO: Possible perf improvement. Instead of going
    // over all the lines each time get diffs of what has
    // changed and update those.
    editor.codeMirror.operation(() => {
      editor.codeMirror.eachLine(lineHandle => {
        const line = fromEditorLine(
          selectedSource.id,
          editor.codeMirror.getLineNumber(lineHandle),
          sourceIsWasm
        );

        if (
          isLineBlackboxed(
            blackboxedRangesForSelectedSource,
            line,
            isSourceOnIgnoreList
          )
        ) {
          this.setBlackboxLine(editor, lineHandle);
        } else {
          this.clearBlackboxLine(editor, lineHandle);
        }
      });
    });
  }

  componentWillUnmount() {
    // Lets make sure we remove everything  relating to
    // blackboxing lines when this component is unmounted.
    this.clearAllBlackboxLines(this.props.editor);
  }

  clearAllBlackboxLines(editor) {
    editor.codeMirror.operation(() => {
      editor.codeMirror.eachLine(lineHandle => {
        this.clearBlackboxLine(editor, lineHandle);
      });
    });
  }

  setAllBlackboxLines(editor) {
    //TODO:We might be able to handle the whole source
    // than adding the blackboxing line by line
    editor.codeMirror.operation(() => {
      editor.codeMirror.eachLine(lineHandle => {
        this.setBlackboxLine(editor, lineHandle);
      });
    });
  }

  clearBlackboxLine(editor, lineHandle) {
    editor.codeMirror.removeLineClass(lineHandle, "wrap", "blackboxed-line");
  }

  setBlackboxLine(editor, lineHandle) {
    editor.codeMirror.addLineClass(lineHandle, "wrap", "blackboxed-line");
  }

  render() {
    return null;
  }
}

export default BlackboxLines;
