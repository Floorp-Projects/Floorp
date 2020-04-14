/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Editor = require("devtools/client/shared/sourceeditor/editor");
const {
  setTargetSearchResult,
} = require("devtools/client/netmonitor/src/actions/search");
const { div } = dom;
/**
 * CodeMirror editor as a React component
 */
class SourcePreview extends Component {
  static get propTypes() {
    return {
      // Source editor syntax highlight mode, which is a mime type defined in CodeMirror
      mode: PropTypes.string,
      // Source editor content
      text: PropTypes.string,
      // Search result text to select
      targetSearchResult: PropTypes.object,
      // Reset target search result that has been used for navigation in this panel.
      // This is done to avoid second navigation the next time.
      resetTargetSearchResult: PropTypes.func,
    };
  }

  componentDidMount() {
    const { mode, text } = this.props;
    this.loadEditor(mode, text);
  }

  shouldComponentUpdate(nextProps) {
    return (
      nextProps.mode !== this.props.mode ||
      nextProps.text !== this.props.text ||
      nextProps.targetSearchResult !== this.props.targetSearchResult
    );
  }

  componentDidUpdate(prevProps) {
    const { mode, targetSearchResult, text } = this.props;

    if (prevProps.text !== text) {
      // When updating from editor to editor
      this.updateEditor(mode, text);
    } else if (prevProps.targetSearchResult !== targetSearchResult) {
      this.findSearchResult();
    }
  }

  componentWillUnmount() {
    this.unloadEditor();
  }

  loadEditor(mode, text) {
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
        this.findSearchResult();
      });
    });
  }

  updateEditor(mode, text) {
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
      this.findSearchResult();
    });
  }

  unloadEditor() {
    clearTimeout(this.editorTimeout);
    clearTimeout(this.editorSetModeTimeout);
    if (this.editor) {
      this.editor.destroy();
      this.editor = null;
    }
  }

  findSearchResult() {
    const { targetSearchResult, resetTargetSearchResult } = this.props;

    if (targetSearchResult?.line) {
      const { line } = targetSearchResult;
      // scroll the editor to center the line
      // with the target search result
      if (this.editor) {
        this.editor.setCursor({ line: line - 1 }, "center");
      }
    }

    resetTargetSearchResult();
  }

  // Scroll to specified line if the user clicks on search results.
  scrollToLine(element) {
    const { targetSearchResult, resetTargetSearchResult } = this.props;

    // The following code is responsible for scrolling given line
    // to visible view-port.
    // It gets the <div> child element representing the target
    // line (by index) and uses `scrollIntoView` API to make sure
    // it's visible to the user.
    if (element && targetSearchResult && targetSearchResult.line) {
      const child = element.children[targetSearchResult.line - 1];
      if (child) {
        const range = document.createRange();
        range.selectNode(child);
        document.getSelection().addRange(range);
        child.scrollIntoView({ block: "center" });
      }
      resetTargetSearchResult();
    }
  }

  renderEditor() {
    return div(
      { className: "editor-row-container" },
      div({
        ref: "editorElement",
        className: "source-editor-mount devtools-monospace",
      })
    );
  }

  render() {
    return div(
      { key: "EDITOR_CONFIG", className: "editor-row-container" },
      this.renderEditor()
    );
  }
}

module.exports = connect(null, dispatch => ({
  resetTargetSearchResult: () => dispatch(setTargetSearchResult(null)),
}))(SourcePreview);
