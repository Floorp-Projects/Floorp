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

export class DebugLine extends PureComponent {
  debugExpression;

  static get propTypes() {
    return {
      location: PropTypes.object,
      why: PropTypes.object,
    };
  }

  componentDidMount() {
    const { why, location } = this.props;
    this.setDebugLine(why, location);
  }

  componentWillUnmount() {
    const { why, location } = this.props;
    this.clearDebugLine(why, location);
  }

  componentDidUpdate(prevProps) {
    const { why, location } = this.props;

    startOperation();
    this.clearDebugLine(prevProps.why, prevProps.location);
    this.setDebugLine(why, location);
    endOperation();
  }

  setDebugLine(why, location) {
    if (!location) {
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

  clearDebugLine(why, location) {
    // Avoid clearing the line if we didn't set a debug line before,
    // or, if the document is no longer available
    if (!location || !hasDocument(location.sourceId)) {
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

function isDocumentReady(location, sourceTextContent) {
  return location && sourceTextContent && hasDocument(location.sourceId);
}

const mapStateToProps = state => {
  // Avoid unecessary intermediate updates when there is no location
  // or the source text content isn't yet fully loaded
  const frame = getVisibleSelectedFrame(state);
  const location = frame?.location;
  if (!location) {
    return {};
  }
  const sourceTextContent = getSourceTextContent(state, location.sourceId);
  if (!isDocumentReady(location, sourceTextContent)) {
    return {};
  }
  return {
    location,
    why: getPauseReason(state, getCurrentThread(state)),
  };
};

export default connect(mapStateToProps)(DebugLine);
