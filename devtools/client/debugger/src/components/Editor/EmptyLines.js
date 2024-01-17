/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { connect } from "devtools/client/shared/vendor/react-redux";
import { Component } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import {
  getSelectedSource,
  getSelectedBreakableLines,
} from "../../selectors/index";
import { fromEditorLine } from "../../utils/editor/index";
import { isWasm } from "../../utils/wasm";

class EmptyLines extends Component {
  static get propTypes() {
    return {
      breakableLines: PropTypes.object.isRequired,
      editor: PropTypes.object.isRequired,
      selectedSource: PropTypes.object.isRequired,
    };
  }

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
        editor.codeMirror.removeLineClass(lineHandle, "wrap", "empty-line");
      });
    });
  }

  shouldComponentUpdate(nextProps) {
    const { breakableLines, selectedSource } = this.props;
    return (
      // Breakable lines are something that evolves over time,
      // but we either have them loaded or not. So only compare the size
      // as sometimes we always get a blank new empty Set instance.
      breakableLines.size != nextProps.breakableLines.size ||
      selectedSource.id != nextProps.selectedSource.id
    );
  }

  disableEmptyLines() {
    const { breakableLines, selectedSource, editor } = this.props;

    const { codeMirror } = editor;
    const isSourceWasm = isWasm(selectedSource.id);

    codeMirror.operation(() => {
      const lineCount = codeMirror.lineCount();
      for (let i = 0; i < lineCount; i++) {
        const line = fromEditorLine(selectedSource.id, i, isSourceWasm);

        if (breakableLines.has(line)) {
          codeMirror.removeLineClass(i, "wrap", "empty-line");
        } else {
          codeMirror.addLineClass(i, "wrap", "empty-line");
        }
      }
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
