/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PureComponent } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import {
  toEditorPosition,
  getTokenEnd,
  hasDocument,
} from "../../utils/editor/index";

import { getIndentation } from "../../utils/indentation";
import { createLocation } from "../../utils/location";

export default class Exception extends PureComponent {
  exceptionLine;
  markText;

  static get propTypes() {
    return {
      exception: PropTypes.object.isRequired,
      doc: PropTypes.object.isRequired,
      selectedSource: PropTypes.string.isRequired,
    };
  }

  componentDidMount() {
    this.addEditorExceptionLine();
  }

  componentDidUpdate() {
    this.clearEditorExceptionLine();
    this.addEditorExceptionLine();
  }

  componentWillUnmount() {
    this.clearEditorExceptionLine();
  }

  setEditorExceptionLine(doc, line, column, lineText) {
    doc.addLineClass(line, "wrap", "line-exception");

    column = Math.max(column, getIndentation(lineText));
    const columnEnd = doc.cm ? getTokenEnd(doc.cm, line, column) : null;

    const markText = doc.markText(
      { ch: column, line },
      { ch: columnEnd, line },
      { className: "mark-text-exception" }
    );

    this.exceptionLine = line;
    this.markText = markText;
  }

  addEditorExceptionLine() {
    const { exception, doc, selectedSource } = this.props;
    const { columnNumber, lineNumber } = exception;

    if (!hasDocument(selectedSource.id)) {
      return;
    }

    const location = createLocation({
      source: selectedSource,
      line: lineNumber,
      // Exceptions are reported with column being 1-based
      // while the frontend uses 0-based column.
      column: columnNumber - 1,
    });

    const { line, column } = toEditorPosition(location);
    const lineText = doc.getLine(line);

    this.setEditorExceptionLine(doc, line, column, lineText);
  }

  clearEditorExceptionLine() {
    if (this.markText) {
      const { selectedSource } = this.props;

      this.markText.clear();

      if (hasDocument(selectedSource.id)) {
        this.props.doc.removeLineClass(
          this.exceptionLine,
          "wrap",
          "line-exception"
        );
      }
      this.exceptionLine = null;
      this.markText = null;
    }
  }

  // This component is only used as a "proxy" to manipulate the editor.
  render() {
    return null;
  }
}
