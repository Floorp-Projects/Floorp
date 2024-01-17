/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import {
  toEditorLine,
  endOperation,
  startOperation,
} from "../../utils/editor/index";
import { getDocument, hasDocument } from "../../utils/editor/source-documents";

import { connect } from "devtools/client/shared/vendor/react-redux";
import {
  getVisibleSelectedFrame,
  getSelectedLocation,
  getSelectedSourceTextContent,
  getPauseCommand,
  getCurrentThread,
} from "../../selectors/index";

function isDebugLine(selectedFrame, selectedLocation) {
  if (!selectedFrame) {
    return false;
  }

  return (
    selectedFrame.location.source.id == selectedLocation.source.id &&
    selectedFrame.location.line == selectedLocation.line
  );
}

function isDocumentReady(selectedLocation, selectedSourceTextContent) {
  return (
    selectedLocation &&
    selectedSourceTextContent &&
    hasDocument(selectedLocation.source.id)
  );
}

export class HighlightLine extends Component {
  isStepping = false;
  previousEditorLine = null;

  static get propTypes() {
    return {
      pauseCommand: PropTypes.oneOf([
        "expression",
        "resume",
        "stepOver",
        "stepIn",
        "stepOut",
      ]),
      selectedFrame: PropTypes.object,
      selectedLocation: PropTypes.object.isRequired,
      selectedSourceTextContent: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    const { selectedLocation, selectedSourceTextContent } = nextProps;
    return this.shouldSetHighlightLine(
      selectedLocation,
      selectedSourceTextContent
    );
  }

  componentDidUpdate(prevProps) {
    this.completeHighlightLine(prevProps);
  }

  componentDidMount() {
    this.completeHighlightLine(null);
  }

  shouldSetHighlightLine(selectedLocation, selectedSourceTextContent) {
    const { line } = selectedLocation;
    const editorLine = toEditorLine(selectedLocation.source.id, line);

    if (!isDocumentReady(selectedLocation, selectedSourceTextContent)) {
      return false;
    }

    if (this.isStepping && editorLine === this.previousEditorLine) {
      return false;
    }

    return true;
  }

  completeHighlightLine(prevProps) {
    const {
      pauseCommand,
      selectedLocation,
      selectedFrame,
      selectedSourceTextContent,
    } = this.props;
    if (pauseCommand) {
      this.isStepping = true;
    }

    startOperation();
    if (prevProps) {
      this.clearHighlightLine(
        prevProps.selectedLocation,
        prevProps.selectedSourceTextContent
      );
    }
    this.setHighlightLine(
      selectedLocation,
      selectedFrame,
      selectedSourceTextContent
    );
    endOperation();
  }

  setHighlightLine(selectedLocation, selectedFrame, selectedSourceTextContent) {
    const { line } = selectedLocation;
    if (
      !this.shouldSetHighlightLine(selectedLocation, selectedSourceTextContent)
    ) {
      return;
    }

    this.isStepping = false;
    const sourceId = selectedLocation.source.id;
    const editorLine = toEditorLine(sourceId, line);
    this.previousEditorLine = editorLine;

    if (!line || isDebugLine(selectedFrame, selectedLocation)) {
      return;
    }

    const doc = getDocument(sourceId);
    doc.addLineClass(editorLine, "wrap", "highlight-line");
    this.resetHighlightLine(doc, editorLine);
  }

  resetHighlightLine(doc, editorLine) {
    const editorWrapper = document.querySelector(".editor-wrapper");

    if (editorWrapper === null) {
      return;
    }

    const duration = parseInt(
      getComputedStyle(editorWrapper).getPropertyValue(
        "--highlight-line-duration"
      ),
      10
    );

    setTimeout(
      () => doc && doc.removeLineClass(editorLine, "wrap", "highlight-line"),
      duration
    );
  }

  clearHighlightLine(selectedLocation, selectedSourceTextContent) {
    if (!isDocumentReady(selectedLocation, selectedSourceTextContent)) {
      return;
    }

    const { line } = selectedLocation;
    const sourceId = selectedLocation.source.id;
    const editorLine = toEditorLine(sourceId, line);
    const doc = getDocument(sourceId);
    doc.removeLineClass(editorLine, "wrap", "highlight-line");
  }

  render() {
    return null;
  }
}

export default connect(state => {
  const selectedLocation = getSelectedLocation(state);

  if (!selectedLocation) {
    throw new Error("must have selected location");
  }
  return {
    pauseCommand: getPauseCommand(state, getCurrentThread(state)),
    selectedFrame: getVisibleSelectedFrame(state),
    selectedLocation,
    selectedSourceTextContent: getSelectedSourceTextContent(state),
  };
})(HighlightLine);
