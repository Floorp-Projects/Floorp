"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.HighlightLine = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _editor = require("../../utils/editor/index");

var _sourceDocuments = require("../../utils/editor/source-documents");

var _source = require("../../utils/source");

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function isDebugLine(selectedFrame, selectedLocation) {
  if (!selectedFrame) {
    return;
  }

  return selectedFrame.location.sourceId == selectedLocation.sourceId && selectedFrame.location.line == selectedLocation.line;
}

function isDocumentReady(selectedSource, selectedLocation) {
  return selectedLocation && (0, _source.isLoaded)(selectedSource) && (0, _sourceDocuments.hasDocument)(selectedLocation.sourceId);
}

class HighlightLine extends _react.Component {
  constructor(...args) {
    var _temp;

    return _temp = super(...args), this.isStepping = false, this.previousEditorLine = null, _temp;
  }

  shouldComponentUpdate(nextProps) {
    const {
      selectedLocation,
      selectedSource
    } = nextProps;
    return this.shouldSetHighlightLine(selectedLocation, selectedSource);
  }

  shouldSetHighlightLine(selectedLocation, selectedSource) {
    const {
      sourceId,
      line
    } = selectedLocation;
    const editorLine = (0, _editor.toEditorLine)(sourceId, line);

    if (!isDocumentReady(selectedSource, selectedLocation)) {
      return false;
    }

    if (this.isStepping && editorLine === this.previousEditorLine) {
      return false;
    }

    return true;
  }

  componentDidUpdate(prevProps) {
    const {
      pauseCommand,
      selectedLocation,
      selectedFrame,
      selectedSource
    } = this.props;

    if (pauseCommand) {
      this.isStepping = true;
    }

    this.clearHighlightLine(prevProps.selectedLocation, prevProps.selectedSource);
    this.setHighlightLine(selectedLocation, selectedFrame, selectedSource);
  }

  setHighlightLine(selectedLocation, selectedFrame, selectedSource) {
    const {
      sourceId,
      line
    } = selectedLocation;

    if (!this.shouldSetHighlightLine(selectedLocation, selectedSource)) {
      return;
    }

    this.isStepping = false;
    const editorLine = (0, _editor.toEditorLine)(sourceId, line);
    this.previousEditorLine = editorLine;

    if (!line || isDebugLine(selectedFrame, selectedLocation)) {
      return;
    }

    const doc = (0, _sourceDocuments.getDocument)(sourceId);
    doc.addLineClass(editorLine, "line", "highlight-line");
  }

  clearHighlightLine(selectedLocation, selectedSource) {
    if (!isDocumentReady(selectedSource, selectedLocation)) {
      return;
    }

    const {
      line,
      sourceId
    } = selectedLocation;
    const editorLine = (0, _editor.toEditorLine)(sourceId, line);
    const doc = (0, _sourceDocuments.getDocument)(sourceId);
    doc.removeLineClass(editorLine, "line", "highlight-line");
  }

  render() {
    return null;
  }

}

exports.HighlightLine = HighlightLine;
exports.default = (0, _reactRedux.connect)(state => ({
  pauseCommand: (0, _selectors.getPauseCommand)(state),
  selectedFrame: (0, _selectors.getVisibleSelectedFrame)(state),
  selectedLocation: (0, _selectors.getSelectedLocation)(state),
  selectedSource: (0, _selectors.getSelectedSource)(state)
}))(HighlightLine);