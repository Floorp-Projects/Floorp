/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { Component } from "react";
import { toEditorLine, endOperation, startOperation } from "../../utils/editor";
import { getDocument, hasDocument } from "../../utils/editor/source-documents";

import { connect } from "../../utils/connect";
import {
  getVisibleSelectedFrame,
  getSelectedLocation,
  getSelectedSourceWithContent,
  getPauseCommand,
  getCurrentThread,
} from "../../selectors";

import type {
  Frame,
  SourceLocation,
  SourceWithContent,
  SourceDocuments,
} from "../../types";
import type { Command } from "../../reducers/types";

type Props = {
  pauseCommand: Command,
  selectedFrame: Frame,
  selectedLocation: SourceLocation,
  selectedSourceWithContent: ?SourceWithContent,
};

function isDebugLine(selectedFrame: Frame, selectedLocation: SourceLocation) {
  if (!selectedFrame) {
    return;
  }

  return (
    selectedFrame.location.sourceId == selectedLocation.sourceId &&
    selectedFrame.location.line == selectedLocation.line
  );
}

function isDocumentReady(
  selectedSourceWithContent: ?SourceWithContent,
  selectedLocation
) {
  return (
    selectedLocation &&
    selectedSourceWithContent &&
    selectedSourceWithContent.content &&
    hasDocument(selectedLocation.sourceId)
  );
}

export class HighlightLine extends Component<Props> {
  isStepping: boolean = false;
  previousEditorLine: ?number = null;

  shouldComponentUpdate(nextProps: Props) {
    const { selectedLocation, selectedSourceWithContent } = nextProps;
    return this.shouldSetHighlightLine(
      selectedLocation,
      selectedSourceWithContent
    );
  }

  componentDidUpdate(prevProps: Props) {
    this.completeHighlightLine(prevProps);
  }

  componentDidMount() {
    this.completeHighlightLine(null);
  }

  shouldSetHighlightLine(
    selectedLocation: SourceLocation,
    selectedSourceWithContent: ?SourceWithContent
  ) {
    const { sourceId, line } = selectedLocation;
    const editorLine = toEditorLine(sourceId, line);

    if (!isDocumentReady(selectedSourceWithContent, selectedLocation)) {
      return false;
    }

    if (this.isStepping && editorLine === this.previousEditorLine) {
      return false;
    }

    return true;
  }

  completeHighlightLine(prevProps: Props | null) {
    const {
      pauseCommand,
      selectedLocation,
      selectedFrame,
      selectedSourceWithContent,
    } = this.props;
    if (pauseCommand) {
      this.isStepping = true;
    }

    startOperation();
    if (prevProps) {
      this.clearHighlightLine(
        prevProps.selectedLocation,
        prevProps.selectedSourceWithContent
      );
    }
    this.setHighlightLine(
      selectedLocation,
      selectedFrame,
      selectedSourceWithContent
    );
    endOperation();
  }

  setHighlightLine(
    selectedLocation: SourceLocation,
    selectedFrame: Frame,
    selectedSourceWithContent: ?SourceWithContent
  ) {
    const { sourceId, line } = selectedLocation;
    if (
      !this.shouldSetHighlightLine(selectedLocation, selectedSourceWithContent)
    ) {
      return;
    }

    this.isStepping = false;
    const editorLine = toEditorLine(sourceId, line);
    this.previousEditorLine = editorLine;

    if (!line || isDebugLine(selectedFrame, selectedLocation)) {
      return;
    }

    const doc = getDocument(sourceId);
    doc.addLineClass(editorLine, "line", "highlight-line");
    this.resetHighlightLine(doc, editorLine);
  }

  resetHighlightLine(doc: SourceDocuments, editorLine: number) {
    const editorWrapper: HTMLElement | null = document.querySelector(
      ".editor-wrapper"
    );

    if (editorWrapper === null) {
      return;
    }

    const style = getComputedStyle(editorWrapper);
    const durationString = style.getPropertyValue("--highlight-line-duration");

    let duration = durationString.match(/\d+/);
    duration = duration.length ? Number(duration[0]) : 0;

    setTimeout(
      () => doc && doc.removeLineClass(editorLine, "line", "highlight-line"),
      duration
    );
  }

  clearHighlightLine(
    selectedLocation: SourceLocation,
    selectedSourceWithContent: ?SourceWithContent
  ) {
    if (!isDocumentReady(selectedSourceWithContent, selectedLocation)) {
      return;
    }

    const { line, sourceId } = selectedLocation;
    const editorLine = toEditorLine(sourceId, line);
    const doc = getDocument(sourceId);
    doc.removeLineClass(editorLine, "line", "highlight-line");
  }

  render() {
    return null;
  }
}

export default connect(state => ({
  pauseCommand: getPauseCommand(state, getCurrentThread(state)),
  selectedFrame: getVisibleSelectedFrame(state),
  selectedLocation: getSelectedLocation(state),
  selectedSourceWithContent: getSelectedSourceWithContent(state),
}))(HighlightLine);
