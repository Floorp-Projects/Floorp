/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PureComponent } from "react";
import PropTypes from "prop-types";
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
  getSourceTextContent,
  getCurrentThread,
} from "../../selectors";

function isDocumentReady(location, sourceTextContent) {
  return location && sourceTextContent && hasDocument(location.sourceId);
}

export class DebugLine extends PureComponent {
  debugExpression;

  static get propTypes() {
    return {
      location: PropTypes.object,
      sourceTextContent: PropTypes.object,
      why: PropTypes.object,
    };
  }

  componentDidMount() {
    const { why, location, sourceTextContent } = this.props;
    this.setDebugLine(why, location, sourceTextContent);
  }

  componentWillUnmount() {
    const { why, location, sourceTextContent } = this.props;
    this.clearDebugLine(why, location, sourceTextContent);
  }

  componentDidUpdate(prevProps) {
    const { why, location, sourceTextContent } = this.props;

    startOperation();
    this.clearDebugLine(
      prevProps.why,
      prevProps.location,
      prevProps.sourceTextContent
    );
    this.setDebugLine(why, location, sourceTextContent);
    endOperation();
  }

  setDebugLine(why, location, sourceTextContent) {
    if (!location || !isDocumentReady(location, sourceTextContent)) {
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

  clearDebugLine(why, location, sourceTextContent) {
    if (!location || !isDocumentReady(location, sourceTextContent)) {
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

  getTextClasses(why) {
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
  const location = frame?.location;
  return {
    location,
    sourceTextContent:
      location && getSourceTextContent(state, location.sourceId),
    why: getPauseReason(state, getCurrentThread(state)),
  };
};

export default connect(mapStateToProps)(DebugLine);
