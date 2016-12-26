/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, DOM, PropTypes } = require("devtools/client/shared/vendor/react");
const SourceEditor = require("devtools/client/sourceeditor/editor");

const { div } = DOM;

/**
 * CodeMirror editor as a React component
 */
const Editor = createClass({
  displayName: "Editor",

  propTypes: {
    // Source editor syntax hightligh mode, which is a mime type defined in CodeMirror
    mode: PropTypes.string,
    // Source editor is displayed if set to true
    open: PropTypes.bool,
    // Source editor content
    text: PropTypes.string,
  },

  getDefaultProps() {
    return {
      mode: null,
      open: true,
      text: "",
    };
  },

  componentDidMount() {
    const { mode, text } = this.props;

    this.editor = new SourceEditor({
      lineNumbers: true,
      mode,
      readOnly: true,
      value: text,
    });

    this.deferEditor = this.editor.appendTo(this.refs.editorElement);
  },

  componentDidUpdate(prevProps) {
    const { mode, open, text } = this.props;

    if (!open) {
      return;
    }

    if (prevProps.mode !== mode) {
      this.deferEditor.then(() => {
        this.editor.setMode(mode);
      });
    }

    if (prevProps.text !== text) {
      this.deferEditor.then(() => {
        // FIXME: Workaround for browser_net_accessibility test to
        // make sure editor node exists while setting editor text.
        // deferEditor workaround should be removed in bug 1308442
        if (this.refs.editorElement) {
          this.editor.setText(text);
        }
      });
    }
  },

  componentWillUnmount() {
    this.deferEditor.then(() => {
      this.editor.destroy();
      this.editor = null;
    });
    this.deferEditor = null;
  },

  render() {
    const { open } = this.props;

    return (
      div({ className: "editor-container devtools-monospace" },
        div({
          ref: "editorElement",
          className: "editor-mount devtools-monospace",
          // Using visibility instead of display property to avoid breaking
          // CodeMirror indentation
          style: { visibility: open ? "visible" : "hidden" },
        }),
      )
    );
  }
});

module.exports = Editor;
