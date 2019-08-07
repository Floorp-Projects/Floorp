/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import ReactDOM from "react-dom";

import assert from "../../utils/assert";
import InlinePreview from "./InlinePreview";

import type { Preview } from "../../types";

type Props = {
  editor: Object,
  line: number,
  preview: Preview,
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

    const { editor, line, preview } = props;

    if (!this.IPWidget) {
      const widget = document.createElement("div");
      widget.classList.add("inline-preview");
      this.IPWidget = editor.codeMirror.addLineWidget(line, widget);
    }

    // Determine the end of line and append preview after leaving gap of 8px
    const left = editor.getLine(line).length * editor.defaultCharWidth() + 8;
    if (!prevProps || this.lastLeft !== left) {
      this.lastLeft = left;
      this.IPWidget.node.style.left = `${left}px`;
    }

    ReactDOM.render(
      <React.Fragment>
        {Object.keys(preview).map((variableName: string) => (
          <InlinePreview
            line={line}
            variable={variableName}
            value={preview[variableName]}
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

export default InlinePreviewRow;
