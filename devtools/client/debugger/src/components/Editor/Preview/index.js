/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React, { PureComponent } from "react";
import { connect } from "../../../utils/connect";

import Popup from "./Popup";

import {
  getPreview,
  getThreadContext,
  getCurrentThread,
  getHighlightedCalls,
  getIsCurrentThreadPaused,
} from "../../../selectors";
import actions from "../../../actions";

const EXCEPTION_MARKER = "mark-text-exception";

class Preview extends PureComponent {
  target = null;
  constructor(props) {
    super(props);
    this.state = { selecting: false };
  }

  static get propTypes() {
    return {
      clearPreview: PropTypes.func.isRequired,
      cx: PropTypes.object.isRequired,
      editor: PropTypes.object.isRequired,
      editorRef: PropTypes.object.isRequired,
      highlightedCalls: PropTypes.array,
      isPaused: PropTypes.bool.isRequired,
      preview: PropTypes.object,
      setExceptionPreview: PropTypes.func.isRequired,
      updatePreview: PropTypes.func.isRequired,
    };
  }

  componentDidMount() {
    this.updateListeners();
  }

  componentWillUnmount() {
    const { codeMirror } = this.props.editor;
    const codeMirrorWrapper = codeMirror.getWrapperElement();

    codeMirror.off("tokenenter", this.onTokenEnter);
    codeMirror.off("scroll", this.onScroll);
    codeMirrorWrapper.removeEventListener("mouseup", this.onMouseUp);
    codeMirrorWrapper.removeEventListener("mousedown", this.onMouseDown);
  }

  updateListeners(prevProps) {
    const { codeMirror } = this.props.editor;
    const codeMirrorWrapper = codeMirror.getWrapperElement();
    codeMirror.on("tokenenter", this.onTokenEnter);
    codeMirror.on("scroll", this.onScroll);
    codeMirrorWrapper.addEventListener("mouseup", this.onMouseUp);
    codeMirrorWrapper.addEventListener("mousedown", this.onMouseDown);
  }

  onTokenEnter = ({ target, tokenPos }) => {
    const {
      cx,
      editor,
      updatePreview,
      highlightedCalls,
      setExceptionPreview,
    } = this.props;

    const isTargetException = target.classList.contains(EXCEPTION_MARKER);

    if (isTargetException) {
      setExceptionPreview(cx, target, tokenPos, editor.codeMirror);
      return;
    }

    if (
      this.props.isPaused &&
      !this.state.selecting &&
      highlightedCalls === null &&
      !isTargetException
    ) {
      updatePreview(cx, target, tokenPos, editor.codeMirror);
    }
  };

  onMouseUp = () => {
    if (this.props.isPaused) {
      this.setState({ selecting: false });
    }
  };

  onMouseDown = () => {
    if (this.props.isPaused) {
      this.setState({ selecting: true });
    }
  };

  onScroll = () => {
    if (this.props.isPaused) {
      this.props.clearPreview(this.props.cx);
    }
  };

  render() {
    const { preview } = this.props;
    if (!preview || this.state.selecting) {
      return null;
    }

    return (
      <Popup
        preview={preview}
        editor={this.props.editor}
        editorRef={this.props.editorRef}
      />
    );
  }
}

const mapStateToProps = state => {
  const thread = getCurrentThread(state);
  return {
    highlightedCalls: getHighlightedCalls(state, thread),
    cx: getThreadContext(state),
    preview: getPreview(state),
    isPaused: getIsCurrentThreadPaused(state),
  };
};

export default connect(mapStateToProps, {
  clearPreview: actions.clearPreview,
  addExpression: actions.addExpression,
  updatePreview: actions.updatePreview,
  setExceptionPreview: actions.setExceptionPreview,
})(Preview);
