/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export * from "./source-documents";
export * from "./get-token-location";
export * from "./source-search";
export * from "../ui";
export { onMouseOver } from "./token-events";

import { createEditor } from "./create-editor";
import { findNext, findPrev } from "./source-search";

import { isWasm, lineToWasmOffset, wasmOffsetToLine } from "../wasm";

import type { AstLocation } from "../../workers/parser";
import type { EditorPosition, EditorRange } from "../editor/types";
import type {
  SearchModifiers,
  Source,
  SourceLocation,
  SourceId,
} from "../../types";
type Editor = Object;

let editor: ?Editor;

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

export function traverseResults(
  e: Event,
  ctx: any,
  query: string,
  dir: string,
  modifiers: SearchModifiers
) {
  e.stopPropagation();
  e.preventDefault();

  if (dir == "prev") {
    findPrev(ctx, query, true, modifiers);
  } else if (dir == "next") {
    findNext(ctx, query, true, modifiers);
  }
}

export function toEditorLine(sourceId: SourceId, lineOrOffset: number): number {
  if (isWasm(sourceId)) {
    // TODO ensure offset is always "mappable" to edit line.
    return wasmOffsetToLine(sourceId, lineOrOffset) || 0;
  }

  return lineOrOffset ? lineOrOffset - 1 : 1;
}

export function fromEditorLine(sourceId: SourceId, line: number): number {
  if (isWasm(sourceId)) {
    return lineToWasmOffset(sourceId, line) || 0;
  }

  return line + 1;
}

export function toEditorPosition(location: SourceLocation): EditorPosition {
  return {
    line: toEditorLine(location.sourceId, location.line),
    column: isWasm(location.sourceId) || !location.column ? 0 : location.column,
  };
}

export function toEditorRange(
  sourceId: SourceId,
  location: AstLocation
): EditorRange {
  const { start, end } = location;
  return {
    start: toEditorPosition({ ...start, sourceId }),
    end: toEditorPosition({ ...end, sourceId }),
  };
}

export function toSourceLine(sourceId: SourceId, line: number): ?number {
  return isWasm(sourceId) ? lineToWasmOffset(sourceId, line) : line + 1;
}

export function scrollToColumn(codeMirror: any, line: number, column: number) {
  const { top, left } = codeMirror.charCoords({ line, ch: column }, "local");

  if (!isVisible(codeMirror, top, left)) {
    const scroller = codeMirror.getScrollerElement();
    const centeredX = Math.max(left - scroller.offsetWidth / 2, 0);
    const centeredY = Math.max(top - scroller.offsetHeight / 2, 0);

    codeMirror.scrollTo(centeredX, centeredY);
  }
}

function isVisible(codeMirror: any, top: number, left: number) {
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
  { codeMirror }: Object,
  // Offset represents an allowance of characters or lines offscreen to improve
  // perceived performance of column breakpoint rendering
  offsetHorizontalCharacters: number = 100,
  offsetVerticalLines: number = 20
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

export function markText(
  { codeMirror }: Object,
  className: string,
  { start, end }: EditorRange
) {
  return codeMirror.markText(
    { ch: start.column, line: start.line },
    { ch: end.column, line: end.line },
    { className }
  );
}

export function lineAtHeight(
  { codeMirror }: Object,
  sourceId: SourceId,
  event: MouseEvent
) {
  const _editorLine = codeMirror.lineAtHeight(event.clientY);
  return toSourceLine(sourceId, _editorLine);
}

export function getSourceLocationFromMouseEvent(
  { codeMirror }: Object,
  source: Source,
  e: MouseEvent
) {
  const { line, ch } = codeMirror.coordsChar({
    left: e.clientX,
    top: e.clientY,
  });
  const sourceId = source.id;

  return {
    sourceId,
    line: fromEditorLine(sourceId, line),
    column: isWasm(sourceId) ? 0 : ch + 1,
  };
}

export function forEachLine(codeMirror: Object, iter: Function) {
  codeMirror.operation(() => {
    codeMirror.doc.iter(0, codeMirror.lineCount(), iter);
  });
}

export function removeLineClass(
  codeMirror: Object,
  line: number,
  className: string
) {
  codeMirror.removeLineClass(line, "wrapClass", className);
}

export function clearLineClass(codeMirror: Object, className: string) {
  forEachLine(codeMirror, line => {
    removeLineClass(codeMirror, line, className);
  });
}

export function getTextForLine(codeMirror: Object, line: number): string {
  return codeMirror.getLine(line - 1).trim();
}

export function getCursorLine(codeMirror: Object): number {
  return codeMirror.getCursor().line;
}

export function getCursorColumn(codeMirror: Object): number {
  return codeMirror.getCursor().ch;
}

export function getTokenEnd(codeMirror: Object, line: number, column: number) {
  const token = codeMirror.getTokenAt({
    line,
    ch: column + 1,
  });
  const tokenString = token.string;

  return tokenString === "{" || tokenString === "[" ? null : token.end;
}
