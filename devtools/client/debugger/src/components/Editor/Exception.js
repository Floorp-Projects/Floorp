/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PureComponent } from "react";

import { toEditorPosition, getTokenEnd } from "../../utils/editor";

import { getIndentation } from "../../utils/indentation";

export default class Exception extends PureComponent {
  exceptionLine;
  markText;

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
    doc.addLineClass(line, "wrapClass", "line-exception");

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
    const { exception, doc, selectedSourceId } = this.props;
    const { columnNumber, lineNumber } = exception;

    const location = {
      column: columnNumber - 1,
      line: lineNumber,
      sourceId: selectedSourceId,
    };

    const { line, column } = toEditorPosition(location);
    const lineText = doc.getLine(line);

    this.setEditorExceptionLine(doc, line, column, lineText);
  }

  clearEditorExceptionLine() {
    if (this.markText) {
      const { doc } = this.props;

      this.markText.clear();
      doc.removeLineClass(this.exceptionLine, "wrapClass", "line-exception");

      this.exceptionLine = null;
      this.markText = null;
    }
  }

  // This component is only used as a "proxy" to manipulate the editor.
  render() {
    return null;
  }
}
