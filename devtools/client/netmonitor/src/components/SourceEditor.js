/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Editor = require("devtools/client/sourceeditor/editor");

const { div } = dom;

/**
 * CodeMirror editor as a React component
 */
class SourceEditor extends Component {
  static get propTypes() {
    return {
      // Source editor syntax highlight mode, which is a mime type defined in CodeMirror
      mode: PropTypes.string,
      // Source editor content
      text: PropTypes.string,
    };
  }

  componentDidMount() {
    const { mode, text } = this.props;

    this.editor = new Editor({
      lineNumbers: true,
      lineWrapping: false,
      mode: null, // Disable auto syntax detection, but then we set mode asynchronously
      readOnly: true,
      theme: "mozilla",
      value: text,
    });

    // Delay to CodeMirror initialization content to prevent UI freezing
    this.editorTimeout = setTimeout(() => {
      this.editor.appendToLocalElement(this.refs.editorElement);
      // CodeMirror's setMode() (syntax highlight) is the performance bottleneck when
      // processing large content, so we enable it asynchronously within the setTimeout
      // to avoid UI blocking. (rendering source code -> drawing syntax highlight)
      this.editorSetModeTimeout = setTimeout(() => {
        this.editor.setMode(mode);
      });
    });
  }

  shouldComponentUpdate(nextProps) {
    return nextProps.mode !== this.props.mode || nextProps.text !== this.props.text;
  }

  componentDidUpdate(prevProps) {
    const { mode, text } = this.props;

    if (prevProps.text !== text) {
      // Reset the existed 'mode' attribute in order to make setText() process faster
      // to prevent drawing unnecessary syntax highlight.
      this.editor.setMode(null);
      this.editor.setText(text);

      // CodeMirror's setMode() (syntax highlight) is the performance bottleneck when
      // processing large content, so we enable it asynchronously within the setTimeout
      // to avoid UI blocking. (rendering source code -> drawing syntax highlight)
      this.editorSetModeTimeout = setTimeout(() => {
        this.editor.setMode(mode);
      });
    }
  }

  componentWillUnmount() {
    clearTimeout(this.editorTimeout);
    clearTimeout(this.editorSetModeTimeout);
    this.editor.destroy();
  }

  render() {
    return (
      div({
        ref: "editorElement",
        className: "source-editor-mount devtools-monospace",
      })
    );
  }
}

module.exports = SourceEditor;
