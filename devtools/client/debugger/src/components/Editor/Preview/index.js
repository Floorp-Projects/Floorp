/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import { connect } from "../../../utils/connect";

import Popup from "./Popup";

import { getPreview, getThreadContext } from "../../../selectors";
import actions from "../../../actions";

import type { ThreadContext } from "../../../types";

import type { Preview as PreviewType } from "../../../reducers/types";

type Props = {
  cx: ThreadContext,
  editor: any,
  editorRef: ?HTMLDivElement,
  preview: ?PreviewType,
  clearPreview: typeof actions.clearPreview,
  addExpression: typeof actions.addExpression,
  updatePreview: typeof actions.updatePreview,
};

type State = {
  selecting: boolean,
};

class Preview extends PureComponent<Props, State> {
  target = null;
  constructor(props) {
    super(props);
    this.state = { selecting: false };
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

  updateListeners(prevProps: ?Props) {
    const { codeMirror } = this.props.editor;
    const codeMirrorWrapper = codeMirror.getWrapperElement();
    codeMirror.on("tokenenter", this.onTokenEnter);
    codeMirror.on("scroll", this.onScroll);
    codeMirrorWrapper.addEventListener("mouseup", this.onMouseUp);
    codeMirrorWrapper.addEventListener("mousedown", this.onMouseDown);
  }

  onTokenEnter = ({ target, tokenPos }) => {
    const { cx, editor, updatePreview } = this.props;

    if (cx.isPaused && !this.state.selecting) {
      updatePreview(cx, target, tokenPos, editor.codeMirror);
    }
  };

  onMouseUp = () => {
    if (this.props.cx.isPaused) {
      this.setState({ selecting: false });
      return true;
    }
  };

  onMouseDown = () => {
    if (this.props.cx.isPaused) {
      this.setState({ selecting: true });
      return true;
    }
  };

  onScroll = () => {
    if (this.props.cx.isPaused) {
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

const mapStateToProps = state => ({
  cx: getThreadContext(state),
  preview: getPreview(state),
});

export default connect(
  mapStateToProps,
  {
    clearPreview: actions.clearPreview,
    addExpression: actions.addExpression,
    updatePreview: actions.updatePreview,
  }
)(Preview);
