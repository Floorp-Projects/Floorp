/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { Component } from "react";
import {
  toEditorPosition,
  getDocument,
  hasDocument,
  startOperation,
  endOperation
} from "../../utils/editor";
import { isLoaded } from "../../utils/source";
import { isException } from "../../utils/pause";
import { getIndentation } from "../../utils/indentation";
import { connect } from "react-redux";
import {
  getVisibleSelectedFrame,
  getPauseReason,
  getSelectedSource
} from "../../selectors";

import type { Frame, Why, Source } from "../../types";

type Props = {
  selectedFrame: Frame,
  why: Why,
  selectedSource: Source
};

type TextClasses = {
  markTextClass: string,
  lineClass: string
};

function isDocumentReady(selectedSource, selectedFrame) {
  return (
    selectedFrame &&
    isLoaded(selectedSource) &&
    hasDocument(selectedFrame.location.sourceId)
  );
}

export class DebugLine extends Component<Props> {
  debugExpression: null;

  componentDidUpdate(prevProps: Props) {
    const { why, selectedFrame, selectedSource } = this.props;

    startOperation();
    this.clearDebugLine(
      prevProps.selectedFrame,
      prevProps.selectedSource,
      prevProps.why
    );
    this.setDebugLine(why, selectedFrame, selectedSource);
    endOperation();
  }

  componentDidMount() {
    const { why, selectedFrame, selectedSource } = this.props;
    this.setDebugLine(why, selectedFrame, selectedSource);
  }

  setDebugLine(why: Why, selectedFrame: Frame, selectedSource: Source) {
    if (!isDocumentReady(selectedSource, selectedFrame)) {
      return;
    }
    const sourceId = selectedFrame.location.sourceId;
    const doc = getDocument(sourceId);

    let { line, column } = toEditorPosition(selectedFrame.location);
    const { markTextClass, lineClass } = this.getTextClasses(why);
    doc.addLineClass(line, "line", lineClass);

    const lineText = doc.getLine(line);
    column = Math.max(column, getIndentation(lineText));

    this.debugExpression = doc.markText(
      { ch: column, line },
      { ch: null, line },
      { className: markTextClass }
    );
  }

  clearDebugLine(selectedFrame: Frame, selectedSource: Source, why: Why) {
    if (!isDocumentReady(selectedSource, selectedFrame)) {
      return;
    }

    if (this.debugExpression) {
      this.debugExpression.clear();
    }

    const sourceId = selectedFrame.location.sourceId;
    const { line } = toEditorPosition(selectedFrame.location);
    const doc = getDocument(sourceId);
    const { lineClass } = this.getTextClasses(why);
    doc.removeLineClass(line, "line", lineClass);
  }

  getTextClasses(why: Why): TextClasses {
    if (isException(why)) {
      return {
        markTextClass: "debug-expression-error",
        lineClass: "new-debug-line-error"
      };
    }

    return { markTextClass: "debug-expression", lineClass: "new-debug-line" };
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => ({
  selectedFrame: getVisibleSelectedFrame(state),
  selectedSource: getSelectedSource(state),
  why: getPauseReason(state)
});

export default connect(mapStateToProps)(DebugLine);
