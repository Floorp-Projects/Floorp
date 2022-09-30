/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const Editor = require("resource://devtools/client/shared/sourceeditor/editor.js");
const {
  setTargetSearchResult,
} = require("resource://devtools/client/netmonitor/src/actions/search.js");
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
      // Auto scroll to specific line
      scrollToLine: PropTypes.number,
      // Reset target search result that has been used for navigation in this panel.
      // This is done to avoid second navigation the next time.
      resetTargetSearchResult: PropTypes.func,
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
      this.editorTimeout = null;
      this.editor.appendToLocalElement(this.refs.editorElement);

      // CodeMirror's setMode() (syntax highlight) is the performance bottleneck when
      // processing large content, so we enable it asynchronously within the setTimeout
      // to avoid UI blocking. (rendering source code -> drawing syntax highlight)
      this.editorSetModeTimeout = setTimeout(() => {
        this.editorSetModeTimeout = null;
        this.editor.setMode(mode);
        this.scrollToLine();
      });
    });
  }

  shouldComponentUpdate(nextProps) {
    return (
      nextProps.mode !== this.props.mode ||
      nextProps.text !== this.props.text ||
      nextProps.scrollToLine !== this.props.scrollToLine
    );
  }

  componentDidUpdate(prevProps) {
    const { mode, scrollToLine, text } = this.props;

    // Bail out if the editor has been destroyed in the meantime.
    if (this.editor.isDestroyed()) {
      return;
    }

    if (prevProps.text !== text) {
      // Reset the existed 'mode' attribute in order to make setText() process faster
      // to prevent drawing unnecessary syntax highlight.
      this.editor.setMode(null);
      this.editor.setText(text);

      if (this.editorSetModeTimeout) {
        clearTimeout(this.editorSetModeTimeout);
      }

      // CodeMirror's setMode() (syntax highlight) is the performance bottleneck when
      // processing large content, so we enable it asynchronously within the setTimeout
      // to avoid UI blocking. (rendering source code -> drawing syntax highlight)
      this.editorSetModeTimeout = setTimeout(() => {
        this.editorSetModeTimeout = null;
        this.editor.setMode(mode);
        this.scrollToLine();
      });
    } else if (prevProps.scrollToLine !== scrollToLine) {
      this.scrollToLine();
    }
  }

  componentWillUnmount() {
    clearTimeout(this.editorTimeout);
    clearTimeout(this.editorSetModeTimeout);
    this.editor.destroy();
  }

  scrollToLine() {
    const { scrollToLine, resetTargetSearchResult } = this.props;

    if (scrollToLine) {
      this.editor.setCursor(
        {
          line: scrollToLine - 1,
        },
        "center"
      );
    }

    resetTargetSearchResult();
  }

  render() {
    return div({
      ref: "editorElement",
      className: "source-editor-mount devtools-monospace",
    });
  }
}

module.exports = connect(null, dispatch => ({
  resetTargetSearchResult: () => dispatch(setTargetSearchResult(null)),
}))(SourceEditor);
