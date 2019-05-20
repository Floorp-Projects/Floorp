/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { PureComponent } from "react";
import {
  toEditorPosition,
  getDocument,
  hasDocument,
  startOperation,
  endOperation,
  getTokenEnd,
} from "../../utils/editor";
import { isException } from "../../utils/pause";
import { getIndentation } from "../../utils/indentation";
import { connect } from "../../utils/connect";
import {
  getVisibleSelectedFrame,
  getPauseReason,
  getSourceWithContent,
  getCurrentThread,
} from "../../selectors";

import type { Frame, Why, SourceWithContent } from "../../types";

type Props = {
  frame: Frame,
  why: Why,
  source: ?SourceWithContent,
};

type TextClasses = {
  markTextClass: string,
  lineClass: string,
};

function isDocumentReady(source: ?SourceWithContent, frame) {
  return (
    frame && source && source.content && hasDocument(frame.location.sourceId)
  );
}

export class DebugLine extends PureComponent<Props> {
  debugExpression: null;

  componentDidMount() {
    const { why, frame, source } = this.props;
    this.setDebugLine(why, frame, source);
  }

  componentWillUnmount() {
    const { why, frame, source } = this.props;
    this.clearDebugLine(why, frame, source);
  }

  componentDidUpdate(prevProps: Props) {
    const { why, frame, source } = this.props;

    startOperation();
    this.clearDebugLine(prevProps.why, prevProps.frame, prevProps.source);
    this.setDebugLine(why, frame, source);
    endOperation();
  }

  setDebugLine(why: Why, frame: Frame, source: ?SourceWithContent) {
    if (!isDocumentReady(source, frame)) {
      return;
    }
    const sourceId = frame.location.sourceId;
    const doc = getDocument(sourceId);

    let { line, column } = toEditorPosition(frame.location);
    const { markTextClass, lineClass } = this.getTextClasses(why);
    doc.addLineClass(line, "line", lineClass);

    const lineText = doc.getLine(line);
    column = Math.max(column, getIndentation(lineText));

    // If component updates because user clicks on
    // another source tab, codeMirror will be null.
    const columnEnd = doc.cm ? getTokenEnd(doc.cm, line, column) : null;

    this.debugExpression = doc.markText(
      { ch: column, line },
      { ch: columnEnd, line },
      { className: markTextClass }
    );
  }

  clearDebugLine(why: Why, frame: Frame, source: ?SourceWithContent) {
    if (!isDocumentReady(source, frame)) {
      return;
    }

    if (this.debugExpression) {
      this.debugExpression.clear();
    }

    const sourceId = frame.location.sourceId;
    const { line } = toEditorPosition(frame.location);
    const doc = getDocument(sourceId);
    const { lineClass } = this.getTextClasses(why);
    doc.removeLineClass(line, "line", lineClass);
  }

  getTextClasses(why: Why): TextClasses {
    if (isException(why)) {
      return {
        markTextClass: "debug-expression-error",
        lineClass: "new-debug-line-error",
      };
    }

    return { markTextClass: "debug-expression", lineClass: "new-debug-line" };
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const frame = getVisibleSelectedFrame(state);
  return {
    frame,
    source: frame && getSourceWithContent(state, frame.location.sourceId),
    why: getPauseReason(state, getCurrentThread(state)),
  };
};

export default connect(mapStateToProps)(DebugLine);
