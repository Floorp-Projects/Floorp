"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _react = require("devtools/client/shared/vendor/react");

var _selectors = require("../../selectors/index");

var _editor = require("../../utils/editor/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class EmptyLines extends _react.Component {
  componentDidMount() {
    this.disableEmptyLines();
  }

  componentDidUpdate() {
    this.disableEmptyLines();
  }

  componentWillUnmount() {
    const {
      emptyLines,
      selectedSource,
      editor
    } = this.props;

    if (!emptyLines) {
      return;
    }

    editor.codeMirror.operation(() => {
      emptyLines.forEach(emptyLine => {
        const line = (0, _editor.toEditorLine)(selectedSource.get("id"), emptyLine);
        editor.codeMirror.removeLineClass(line, "line", "empty-line");
      });
    });
  }

  disableEmptyLines() {
    const {
      emptyLines,
      selectedSource,
      editor
    } = this.props;

    if (!emptyLines) {
      return;
    }

    editor.codeMirror.operation(() => {
      emptyLines.forEach(emptyLine => {
        const line = (0, _editor.toEditorLine)(selectedSource.get("id"), emptyLine);
        editor.codeMirror.addLineClass(line, "line", "empty-line");
      });
    });
  }

  render() {
    return null;
  }

}

const mapStateToProps = state => {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const foundEmptyLines = (0, _selectors.getEmptyLines)(state, selectedSource.id);
  return {
    selectedSource,
    emptyLines: selectedSource ? foundEmptyLines : []
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps)(EmptyLines);