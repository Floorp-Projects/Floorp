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

type OwnProps = {|
  editor: Object,
  line: number,
  previews: Array<Preview>,
|};
type Props = {
  editor: Object,
  line: number,
  previews: Array<Preview>,
  openElementInInspector: typeof actions.openElementInInspectorCommand,
  highlightDomElement: typeof actions.highlightDomElement,
  unHighlightDomElement: typeof actions.unHighlightDomElement,
};

import "./InlinePreview.css";

// Handles rendering for each line ( row )
// * Renders single widget for each line in codemirror
// * Renders InlinePreview for each preview inside the widget
class InlinePreviewRow extends PureComponent<Props> {
  bookmark: Object;
  widgetNode: Object;

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
      this.bookmark &&
      prevProps &&
      (!props ||
        prevProps.editor !== props.editor ||
        prevProps.line !== props.line)
    ) {
      this.bookmark.clear();
      this.bookmark = null;
      this.widgetNode = null;
    }

    if (!props) {
      return assert(
        !this.bookmark,
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

    if (!this.bookmark) {
      this.widgetNode = document.createElement("div");
      this.widgetNode.classList.add("inline-preview");
    }

    ReactDOM.render(
      <React.Fragment>
        {previews.map((preview: Preview) => (
          <InlinePreview
            line={line}
            key={`${line}-${preview.name}`}
            variable={preview.name}
            value={preview.value}
            openElementInInspector={openElementInInspector}
            highlightDomElement={highlightDomElement}
            unHighlightDomElement={unHighlightDomElement}
          />
        ))}
      </React.Fragment>,
      this.widgetNode
    );

    this.bookmark = editor.codeMirror.setBookmark(
      {
        line,
        ch: Infinity,
      },
      this.widgetNode
    );
  }

  render() {
    return null;
  }
}

export default connect<Props, OwnProps, _, _, _, _>(
  () => ({}),
  {
    openElementInInspector: actions.openElementInInspectorCommand,
    highlightDomElement: actions.highlightDomElement,
    unHighlightDomElement: actions.unHighlightDomElement,
  }
)(InlinePreviewRow);
