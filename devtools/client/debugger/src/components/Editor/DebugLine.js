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
  getPausePreviewLocation,
} from "../../selectors";

import type { SourceLocation, Why, SourceWithContent } from "../../types";

type OwnProps = {||};
type Props = {
  location: ?SourceLocation,
  why: ?Why,
  source: ?SourceWithContent,
};

type TextClasses = {
  markTextClass: string,
  lineClass: string,
};

function isDocumentReady(
  source: ?SourceWithContent,
  location: ?SourceLocation
) {
  return location && source && source.content && hasDocument(location.sourceId);
}

export class DebugLine extends PureComponent<Props> {
  debugExpression: null;

  componentDidMount() {
    const { why, location, source } = this.props;
    this.setDebugLine(why, location, source);
  }

  componentWillUnmount() {
    const { why, location, source } = this.props;
    this.clearDebugLine(why, location, source);
  }

  componentDidUpdate(prevProps: Props) {
    const { why, location, source } = this.props;

    startOperation();
    this.clearDebugLine(prevProps.why, prevProps.location, prevProps.source);
    this.setDebugLine(why, location, source);
    endOperation();
  }

  setDebugLine(
    why: ?Why,
    location: ?SourceLocation,
    source: ?SourceWithContent
  ) {
    if (!location || !isDocumentReady(source, location)) {
      return;
    }
    const { sourceId } = location;
    const doc = getDocument(sourceId);

    let { line, column } = toEditorPosition(location);
    let { markTextClass, lineClass } = this.getTextClasses(why);
    doc.addLineClass(line, "wrapClass", lineClass);

    const lineText = doc.getLine(line);
    column = Math.max(column, getIndentation(lineText));

    // If component updates because user clicks on
    // another source tab, codeMirror will be null.
    const columnEnd = doc.cm ? getTokenEnd(doc.cm, line, column) : null;

    if (columnEnd === null) {
      markTextClass += " to-line-end";
    }

    this.debugExpression = doc.markText(
      { ch: column, line },
      { ch: columnEnd, line },
      { className: markTextClass }
    );
  }

  clearDebugLine(
    why: ?Why,
    location: ?SourceLocation,
    source: ?SourceWithContent
  ) {
    if (!location || !isDocumentReady(source, location)) {
      return;
    }

    if (this.debugExpression) {
      this.debugExpression.clear();
    }

    const { line } = toEditorPosition(location);
    const doc = getDocument(location.sourceId);
    const { lineClass } = this.getTextClasses(why);
    doc.removeLineClass(line, "wrapClass", lineClass);
  }

  getTextClasses(why: ?Why): TextClasses {
    if (why && isException(why)) {
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
  const previewLocation = getPausePreviewLocation(state);
  const location = previewLocation || frame?.location;
  return {
    frame,
    location,
    source: location && getSourceWithContent(state, location.sourceId),
    why: getPauseReason(state, getCurrentThread(state)),
  };
};

export default connect<Props, OwnProps, _, _, _, _>(mapStateToProps)(DebugLine);
