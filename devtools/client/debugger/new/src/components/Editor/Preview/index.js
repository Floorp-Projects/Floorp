/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import { connect } from "../../../utils/connect";

import Popup from "./Popup";

import {
  getPreview,
  getSelectedSource,
  getIsPaused,
  getCurrentThread
} from "../../../selectors";
import actions from "../../../actions";
import { toEditorRange } from "../../../utils/editor";

import type { Source } from "../../../types";

import type { Preview as PreviewType } from "../../../reducers/ast";

type Props = {
  editor: any,
  editorRef: ?HTMLDivElement,
  selectedSource: Source,
  preview: PreviewType,
  isPaused: boolean,
  clearPreview: typeof actions.clearPreview,
  setPopupObjectProperties: typeof actions.setPopupObjectProperties,
  addExpression: typeof actions.addExpression,
  updatePreview: typeof actions.updatePreview
};

type State = {
  selecting: boolean
};

function inPopup(e) {
  const { relatedTarget } = e;

  if (!relatedTarget) {
    return true;
  }

  const pop =
    relatedTarget.closest(".tooltip") || relatedTarget.closest(".popover");

  return pop;
}

function getElementFromPos(pos: DOMRect) {
  // We need to use element*s*AtPoint because the tooltip overlays
  // the token and thus an undesirable element may be returned
  const elementsAtPoint = [
    // $FlowIgnore
    ...document.elementsFromPoint(pos.x + pos.width / 2, pos.y + pos.height / 2)
  ];

  return elementsAtPoint.find(el => el.className.startsWith("cm-"));
}

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

    codeMirror.off("scroll", this.onScroll);
    codeMirror.off("tokenenter", this.onTokenEnter);
    codeMirror.off("tokenleave", this.onTokenLeave);
    codeMirrorWrapper.removeEventListener("mouseup", this.onMouseUp);
    codeMirrorWrapper.removeEventListener("mousedown", this.onMouseDown);
  }

  componentDidUpdate(prevProps) {
    this.updateHighlight(prevProps);
  }

  updateListeners(prevProps: ?Props) {
    const { codeMirror } = this.props.editor;
    const codeMirrorWrapper = codeMirror.getWrapperElement();
    codeMirror.on("scroll", this.onScroll);
    codeMirror.on("tokenenter", this.onTokenEnter);
    codeMirror.on("tokenleave", this.onTokenLeave);
    codeMirrorWrapper.addEventListener("mouseup", this.onMouseUp);
    codeMirrorWrapper.addEventListener("mousedown", this.onMouseDown);
  }

  updateHighlight(prevProps) {
    const { preview } = this.props;

    if (preview && !preview.updating) {
      const target = getElementFromPos(preview.cursorPos);
      target && target.classList.add("preview-selection");
    }

    if (prevProps.preview && !prevProps.preview.updating) {
      const target = getElementFromPos(prevProps.preview.cursorPos);
      target && target.classList.remove("preview-selection");
    }
  }

  onTokenEnter = ({ target, tokenPos }) => {
    if (this.props.isPaused) {
      this.props.updatePreview(target, tokenPos, this.props.editor.codeMirror);
    }
  };

  onTokenLeave = e => {
    if (this.props.isPaused && !inPopup(e)) {
      this.props.clearPreview();
    }
  };

  onMouseUp = () => {
    if (this.props.isPaused) {
      this.setState({ selecting: false });
      return true;
    }
  };

  onMouseDown = () => {
    if (this.props.isPaused) {
      this.setState({ selecting: true });
      return true;
    }
  };

  onScroll = () => {
    if (this.props.isPaused) {
      this.props.clearPreview();
    }
  };

  onClose = e => {
    if (this.props.isPaused) {
      this.props.clearPreview();
    }
  };

  render() {
    const { selectedSource, preview } = this.props;
    if (!this.props.editor || !selectedSource || this.state.selecting) {
      return null;
    }

    if (!preview || preview.updating) {
      return null;
    }

    const { result, expression, location, cursorPos } = preview;
    const value = result;
    if (typeof value == "undefined" || value.optimizedOut) {
      return null;
    }

    const editorRange = toEditorRange(selectedSource.id, location);

    return (
      <Popup
        value={value}
        editor={this.props.editor}
        editorRef={this.props.editorRef}
        range={editorRange}
        expression={expression}
        popoverPos={cursorPos}
        onClose={this.onClose}
      />
    );
  }
}

const mapStateToProps = state => ({
  preview: getPreview(state),
  isPaused: getIsPaused(state, getCurrentThread(state)),
  selectedSource: getSelectedSource(state)
});

export default connect(
  mapStateToProps,
  {
    clearPreview: actions.clearPreview,
    setPopupObjectProperties: actions.setPopupObjectProperties,
    addExpression: actions.addExpression,
    updatePreview: actions.updatePreview
  }
)(Preview);
