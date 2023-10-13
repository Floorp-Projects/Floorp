/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React, { PureComponent } from "react";
import { connect } from "../../../utils/connect";

import Popup from "./Popup";

import { getIsCurrentThreadPaused } from "../../../selectors";
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
      editor: PropTypes.object.isRequired,
      editorRef: PropTypes.object.isRequired,
      isPaused: PropTypes.bool.isRequired,
      getExceptionPreview: PropTypes.func.isRequired,
      getPreview: PropTypes.func,
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

  // Note that these events are emitted by utils/editor/tokens.js
  onTokenEnter = async ({ target, tokenPos }) => {
    // Use a temporary object to uniquely identify the asynchronous processing of this user event
    // and bail out if we started hovering another token.
    const tokenId = {};
    this.currentTokenId = tokenId;

    const { editor, getPreview, getExceptionPreview } = this.props;

    // Ignore inline previews code widgets
    if (target.closest(".CodeMirror-widget")) {
      return;
    }

    const isTargetException = target.classList.contains(EXCEPTION_MARKER);

    let preview;
    if (isTargetException) {
      preview = await getExceptionPreview(target, tokenPos, editor.codeMirror);
    }

    if (!preview && this.props.isPaused && !this.state.selecting) {
      preview = await getPreview(target, tokenPos, editor.codeMirror);
    }

    // Prevent modifying state and showing this preview if we started hovering another token
    if (!preview || this.currentTokenId !== tokenId) {
      return;
    }
    this.setState({ preview });
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
      this.clearPreview();
    }
  };

  clearPreview = () => {
    this.setState({ preview: null });
  };

  render() {
    const { preview } = this.state;
    if (!preview || this.state.selecting) {
      return null;
    }
    return React.createElement(Popup, {
      preview: preview,
      editor: this.props.editor,
      editorRef: this.props.editorRef,
      clearPreview: this.clearPreview,
    });
  }
}

const mapStateToProps = state => {
  return {
    isPaused: getIsCurrentThreadPaused(state),
  };
};

export default connect(mapStateToProps, {
  addExpression: actions.addExpression,
  getPreview: actions.getPreview,
  getExceptionPreview: actions.getExceptionPreview,
})(Preview);
