/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export * from "./source-documents";
export * from "./source-search";
export * from "../ui";
export * from "./tokens";

import { createEditor } from "./create-editor";

import { isWasm, lineToWasmOffset, wasmOffsetToLine } from "../wasm";
import { createLocation } from "../location";

let editor;

export function getEditor() {
  if (editor) {
    return editor;
  }

  editor = createEditor();
  return editor;
}

export function removeEditor() {
  editor = null;
}

function getCodeMirror() {
  return editor && editor.hasCodeMirror ? editor.codeMirror : null;
}

export function startOperation() {
  const codeMirror = getCodeMirror();
  if (!codeMirror) {
    return;
  }

  codeMirror.startOperation();
}

export function endOperation() {
  const codeMirror = getCodeMirror();
  if (!codeMirror) {
    return;
  }

  codeMirror.endOperation();
}

export function toEditorLine(sourceId, lineOrOffset) {
  if (isWasm(sourceId)) {
    // TODO ensure offset is always "mappable" to edit line.
    return wasmOffsetToLine(sourceId, lineOrOffset) || 0;
  }

  return lineOrOffset ? lineOrOffset - 1 : 1;
}

export function fromEditorLine(sourceId, line, sourceIsWasm) {
  if (sourceIsWasm) {
    return lineToWasmOffset(sourceId, line) || 0;
  }

  return line + 1;
}

export function toEditorPosition(location) {
  // Note that Spidermonkey, Debugger frontend and CodeMirror are all consistant regarding column
  // and are 0-based. But only CodeMirror consider the line to be 0-based while the two others
  // consider lines to be 1-based.
  return {
    line: toEditorLine(location.source.id, location.line),
    column:
      isWasm(location.source.id) || (!location.column ? 0 : location.column),
  };
}

export function toSourceLine(sourceId, line) {
  return isWasm(sourceId) ? lineToWasmOffset(sourceId, line) : line + 1;
}

export function scrollToPosition(codeMirror, line, column) {
  // For all cases where these are on the first line and column,
  // avoid the possibly slow computation of cursor location on large bundles.
  if (!line && !column) {
    codeMirror.scrollTo(0, 0);
    return;
  }

  const { top, left } = codeMirror.charCoords({ line, ch: column }, "local");

  if (!isVisible(codeMirror, top, left)) {
    const scroller = codeMirror.getScrollerElement();
    const centeredX = Math.max(left - scroller.offsetWidth / 2, 0);
    const centeredY = Math.max(top - scroller.offsetHeight / 2, 0);

    codeMirror.scrollTo(centeredX, centeredY);
  }
}

function isVisible(codeMirror, top, left) {
  function withinBounds(x, min, max) {
    return x >= min && x <= max;
  }

  const scrollArea = codeMirror.getScrollInfo();
  const charWidth = codeMirror.defaultCharWidth();
  const fontHeight = codeMirror.defaultTextHeight();
  const { scrollTop, scrollLeft } = codeMirror.doc;

  const inXView = withinBounds(
    left,
    scrollLeft,
    scrollLeft + (scrollArea.clientWidth - 30) - charWidth
  );

  const inYView = withinBounds(
    top,
    scrollTop,
    scrollTop + scrollArea.clientHeight - fontHeight
  );

  return inXView && inYView;
}

export function getLocationsInViewport(
  { codeMirror },
  // Offset represents an allowance of characters or lines offscreen to improve
  // perceived performance of column breakpoint rendering
  offsetHorizontalCharacters = 100,
  offsetVerticalLines = 20
) {
  // Get scroll position
  if (!codeMirror) {
    return {
      start: { line: 0, column: 0 },
      end: { line: 0, column: 0 },
    };
  }
  const charWidth = codeMirror.defaultCharWidth();
  const scrollArea = codeMirror.getScrollInfo();
  const { scrollLeft } = codeMirror.doc;
  const rect = codeMirror.getWrapperElement().getBoundingClientRect();
  const topVisibleLine =
    codeMirror.lineAtHeight(rect.top, "window") - offsetVerticalLines;
  const bottomVisibleLine =
    codeMirror.lineAtHeight(rect.bottom, "window") + offsetVerticalLines;

  const leftColumn = Math.floor(
    scrollLeft > 0 ? scrollLeft / charWidth - offsetHorizontalCharacters : 0
  );
  const rightPosition = scrollLeft + (scrollArea.clientWidth - 30);
  const rightCharacter =
    Math.floor(rightPosition / charWidth) + offsetHorizontalCharacters;

  return {
    start: {
      line: topVisibleLine || 0,
      column: leftColumn || 0,
    },
    end: {
      line: bottomVisibleLine || 0,
      column: rightCharacter,
    },
  };
}

export function markText({ codeMirror }, className, { start, end }) {
  return codeMirror.markText(
    { ch: start.column, line: start.line },
    { ch: end.column, line: end.line },
    { className }
  );
}

export function lineAtHeight({ codeMirror }, sourceId, event) {
  const _editorLine = codeMirror.lineAtHeight(event.clientY);
  return toSourceLine(sourceId, _editorLine);
}

export function getSourceLocationFromMouseEvent({ codeMirror }, source, e) {
  const { line, ch } = codeMirror.coordsChar({
    left: e.clientX,
    top: e.clientY,
  });

  return createLocation({
    source,
    line: fromEditorLine(source.id, line, isWasm(source.id)),
    column: isWasm(source.id) ? 0 : ch + 1,
  });
}

export function forEachLine(codeMirror, iter) {
  codeMirror.operation(() => {
    codeMirror.doc.iter(0, codeMirror.lineCount(), iter);
  });
}

export function removeLineClass(codeMirror, line, className) {
  codeMirror.removeLineClass(line, "wrap", className);
}

export function clearLineClass(codeMirror, className) {
  forEachLine(codeMirror, line => {
    removeLineClass(codeMirror, line, className);
  });
}

export function getTextForLine(codeMirror, line) {
  return codeMirror.getLine(line - 1).trim();
}

export function getCursorLine(codeMirror) {
  return codeMirror.getCursor().line;
}

export function getCursorColumn(codeMirror) {
  return codeMirror.getCursor().ch;
}
