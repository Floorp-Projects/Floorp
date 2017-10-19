/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const Editor = require("devtools/client/sourceeditor/editor");
const { SOURCE_EDITOR_SYNTAX_HIGHLIGHT_MAX_SIZE } = require("../constants");

const { div } = DOM;

/**
 * CodeMirror editor as a React component
 */
const SourceEditor = createClass({
  displayName: "SourceEditor",

  propTypes: {
    // Source editor syntax hightligh mode, which is a mime type defined in CodeMirror
    mode: PropTypes.string,
    // Source editor content
    text: PropTypes.string,
  },

  componentDidMount() {
    const { mode, text } = this.props;

    this.editor = new Editor({
      lineNumbers: true,
      lineWrapping: false,
      mode: text.length < SOURCE_EDITOR_SYNTAX_HIGHLIGHT_MAX_SIZE ? mode : null,
      readOnly: true,
      theme: "mozilla",
      value: text,
    });

    // Delay to CodeMirror initialization content to prevent UI freezed
    this.editorTimeout = setTimeout(() => {
      this.editor.appendToLocalElement(this.refs.editorElement);
    });
  },

  componentDidUpdate(prevProps) {
    const { mode, text } = this.props;

    if (prevProps.mode !== mode &&
        text.length < SOURCE_EDITOR_SYNTAX_HIGHLIGHT_MAX_SIZE) {
      this.editor.setMode(mode);
    }

    if (prevProps.text !== text) {
      this.editor.setText(text);
    }
  },

  componentWillUnmount() {
    clearTimeout(this.editorTimeout);
    this.editor.destroy();
  },

  render() {
    return (
      div({
        ref: "editorElement",
        className: "source-editor-mount devtools-monospace",
      })
    );
  }
});

module.exports = SourceEditor;
