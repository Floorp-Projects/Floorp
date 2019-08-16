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
  selectedSource: ?SourceWithContent,
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

function isDocumentReady(selectedSource: ?SourceWithContent, selectedLocation) {
  return (
    selectedLocation &&
    selectedSource &&
    selectedSource.content &&
    hasDocument(selectedLocation.sourceId)
  );
}

export class HighlightLine extends Component<Props> {
  isStepping: boolean = false;
  previousEditorLine: ?number = null;

  shouldComponentUpdate(nextProps: Props) {
    const { selectedLocation, selectedSource } = nextProps;
    return this.shouldSetHighlightLine(selectedLocation, selectedSource);
  }

  componentDidUpdate(prevProps: Props) {
    this.completeHighlightLine(prevProps);
  }

  componentDidMount() {
    this.completeHighlightLine(null);
  }

  shouldSetHighlightLine(
    selectedLocation: SourceLocation,
    selectedSource: ?SourceWithContent
  ) {
    const { sourceId, line } = selectedLocation;
    const editorLine = toEditorLine(sourceId, line);

    if (!isDocumentReady(selectedSource, selectedLocation)) {
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
      selectedSource,
    } = this.props;
    if (pauseCommand) {
      this.isStepping = true;
    }

    startOperation();
    if (prevProps) {
      this.clearHighlightLine(
        prevProps.selectedLocation,
        prevProps.selectedSource
      );
    }
    this.setHighlightLine(selectedLocation, selectedFrame, selectedSource);
    endOperation();
  }

  setHighlightLine(
    selectedLocation: SourceLocation,
    selectedFrame: Frame,
    selectedSource: ?SourceWithContent
  ) {
    const { sourceId, line } = selectedLocation;
    if (!this.shouldSetHighlightLine(selectedLocation, selectedSource)) {
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
    selectedSource: ?SourceWithContent
  ) {
    if (!isDocumentReady(selectedSource, selectedLocation)) {
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
  selectedSource: getSelectedSourceWithContent(state),
}))(HighlightLine);
