/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  toEditorLine,
  toEditorPosition,
  toSourceLine,
  scrollToPosition,
  markText,
  lineAtHeight,
  getSourceLocationFromMouseEvent,
  forEachLine,
  removeLineClass,
  clearLineClass,
  getTextForLine,
  getCursorLine,
} from "../index";

import { makeMockSource } from "../../test-mockup";

describe("toEditorLine", () => {
  it("returns an editor line", () => {
    const testId = "test-123";
    const line = 30;
    expect(toEditorLine(testId, line)).toEqual(29);
  });
});

describe("toEditorPosition", () => {
  it("returns an editor position", () => {
    const loc = { source: { id: "source" }, line: 100, column: 25 };
    expect(toEditorPosition(loc)).toEqual({
      line: 99,
      column: 25,
    });
  });
});

describe("toSourceLine", () => {
  it("returns a source line", () => {
    const testId = "test-123";
    const line = 30;
    expect(toSourceLine(testId, line)).toEqual(31);
  });
});

const codeMirror = {
  doc: {
    iter: jest.fn((_, __, cb) => cb()),
  },
  lineCount: jest.fn(() => 100),
  getLine: jest.fn(() => "something"),
  getCursor: jest.fn(() => ({ line: 3 })),
  getScrollerElement: jest.fn(() => ({
    offsetWidth: 100,
    offsetHeight: 100,
  })),
  getScrollInfo: () => ({
    top: 0,
    right: 0,
    bottom: 0,
    left: 0,
    clientHeight: 100,
    clientWidth: 100,
  }),
  removeLineClass: jest.fn(),
  operation: jest.fn(cb => cb()),
  charCoords: jest.fn(() => ({
    top: 100,
    right: 50,
    bottom: 100,
    left: 50,
  })),
  coordsChar: jest.fn(() => ({ line: 6, ch: 30 })),
  lineAtHeight: jest.fn(() => 300),
  markText: jest.fn(),
  scrollTo: jest.fn(),
  defaultCharWidth: jest.fn(() => 8),
  defaultTextHeight: jest.fn(() => 16),
};

const editor = { codeMirror };

describe("scrollToPosition", () => {
  it("calls codemirror APIs charCoords, getScrollerElement, scrollTo", () => {
    scrollToPosition(codeMirror, 60, 123);
    expect(codeMirror.charCoords).toHaveBeenCalledWith(
      { line: 60, ch: 123 },
      "local"
    );
    expect(codeMirror.scrollTo).toHaveBeenCalledWith(0, 50);
  });
});

describe("markText", () => {
  it("calls codemirror API markText & returns marker", () => {
    const loc = {
      start: { line: 10, column: 0 },
      end: { line: 30, column: 50 },
    };
    markText(editor, "test-123", loc);
    expect(codeMirror.markText).toHaveBeenCalledWith(
      { ch: loc.start.column, line: loc.start.line },
      { ch: loc.end.column, line: loc.end.line },
      { className: "test-123" }
    );
  });
});

describe("lineAtHeight", () => {
  it("calls codemirror API lineAtHeight", () => {
    const e = { clientX: 30, clientY: 60 };
    expect(lineAtHeight(editor, "test-123", e)).toEqual(301);
    expect(editor.codeMirror.lineAtHeight).toHaveBeenCalledWith(e.clientY);
  });
});

describe("getSourceLocationFromMouseEvent", () => {
  it("calls codemirror API coordsChar & returns location", () => {
    const source = makeMockSource(undefined, "test-123");
    const e = { clientX: 30, clientY: 60 };
    expect(getSourceLocationFromMouseEvent(editor, source, e)).toEqual({
      source,
      line: 7,
      column: 31,
      sourceActorId: undefined,
      sourceActor: null,
    });
    expect(editor.codeMirror.coordsChar).toHaveBeenCalledWith({
      left: 30,
      top: 60,
    });
  });
});

describe("forEachLine", () => {
  it("calls codemirror API operation && doc.iter across a doc", () => {
    const test = jest.fn();
    forEachLine(codeMirror, test);
    expect(codeMirror.operation).toHaveBeenCalled();
    expect(codeMirror.doc.iter).toHaveBeenCalledWith(0, 100, test);
  });
});

describe("removeLineClass", () => {
  it("calls codemirror API removeLineClass", () => {
    const line = 3;
    const className = "test-class";
    removeLineClass(codeMirror, line, className);
    expect(codeMirror.removeLineClass).toHaveBeenCalledWith(
      line,
      "wrap",
      className
    );
  });
});

describe("clearLineClass", () => {
  it("Uses forEachLine & removeLineClass to clear class on all lines", () => {
    codeMirror.operation.mockClear();
    codeMirror.doc.iter.mockClear();
    codeMirror.removeLineClass.mockClear();
    clearLineClass(codeMirror, "test-class");
    expect(codeMirror.operation).toHaveBeenCalled();
    expect(codeMirror.doc.iter).toHaveBeenCalledWith(
      0,
      100,
      expect.any(Function)
    );
    expect(codeMirror.removeLineClass).toHaveBeenCalled();
  });
});

describe("getTextForLine", () => {
  it("calls codemirror API getLine & returns line text", () => {
    getTextForLine(codeMirror, 3);
    expect(codeMirror.getLine).toHaveBeenCalledWith(2);
  });
});
describe("getCursorLine", () => {
  it("calls codemirror API getCursor & returns line number", () => {
    getCursorLine(codeMirror);
    expect(codeMirror.getCursor).toHaveBeenCalled();
  });
});
