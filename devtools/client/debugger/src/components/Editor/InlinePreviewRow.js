/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import ReactDOM from "react-dom";

import actions from "../../actions";
import assert from "../../utils/assert";
import { connect } from "../../utils/connect";
import InlinePreview from "./InlinePreview";

import type { Preview } from "../../types";

type Props = {
  editor: Object,
  line: number,
  previews: Array<Preview>,
  // Represents the number of column breakpoints to help with preview
  // positioning
  numColumnBreakpoints: number,
  openElementInInspector: typeof actions.openElementInInspectorCommand,
  highlightDomElement: typeof actions.highlightDomElement,
  unHighlightDomElement: typeof actions.unHighlightDomElement,
};

import "./InlinePreview.css";

// Handles rendering for each line ( row )
// * Renders single widget for each line in codemirror
// * Renders InlinePreview for each preview inside the widget
class InlinePreviewRow extends PureComponent<Props> {
  IPWidget: Object;
  lastLeft: number;

  componentDidMount() {
    this.updatePreviewWidget(this.props, null);
  }

  componentDidUpdate(prevProps: Props) {
    this.updatePreviewWidget(this.props, prevProps);
  }

  componentWillUnmount() {
    this.updatePreviewWidget(null, this.props);
  }

  getPreviewPosition(editor: Object, line: number) {
    const lineStartPos = editor.codeMirror.cursorCoords({ line, ch: 0 });
    const lineEndPos = editor.codeMirror.cursorCoords({
      line,
      ch: editor.getLine(line).length,
    });
    const previewSpacing = 8;
    return lineEndPos.left - lineStartPos.left + previewSpacing;
  }

  updatePreviewWidget(props: Props | null, prevProps: Props | null) {
    if (
      this.IPWidget &&
      prevProps &&
      (!props ||
        prevProps.editor !== props.editor ||
        prevProps.line !== props.line)
    ) {
      this.IPWidget.clear();
      this.IPWidget = null;
    }

    if (!props) {
      return assert(
        !this.IPWidget,
        "Inline Preview widget shouldn't be present."
      );
    }

    const {
      editor,
      line,
      previews,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
    } = props;

    if (!this.IPWidget) {
      const widget = document.createElement("div");
      widget.classList.add("inline-preview");
      this.IPWidget = editor.codeMirror.addLineWidget(line, widget);
    }

    const left = this.getPreviewPosition(editor, line);
    if (!prevProps || this.lastLeft !== left) {
      this.lastLeft = left;
      this.IPWidget.node.style.left = `${left}px`;
    }

    ReactDOM.render(
      <React.Fragment>
        {previews.map((preview: Preview) => (
          <InlinePreview
            line={line}
            variable={preview.name}
            value={preview.value}
            openElementInInspector={openElementInInspector}
            highlightDomElement={highlightDomElement}
            unHighlightDomElement={unHighlightDomElement}
          />
        ))}
      </React.Fragment>,
      this.IPWidget.node
    );
  }

  render() {
    return null;
  }
}

export default connect(
  () => ({}),
  {
    openElementInInspector: actions.openElementInInspectorCommand,
    highlightDomElement: actions.highlightDomElement,
    unHighlightDomElement: actions.unHighlightDomElement,
  }
)(InlinePreviewRow);
