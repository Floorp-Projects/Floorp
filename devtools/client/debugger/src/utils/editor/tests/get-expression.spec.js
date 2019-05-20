/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import CodeMirror from "codemirror";
import { getExpressionFromCoords } from "../get-expression";

describe("get-expression", () => {
  let isCreateTextRangeDefined;

  beforeAll(() => {
    if ((document.body: any).createTextRange) {
      isCreateTextRangeDefined = true;
    } else {
      isCreateTextRangeDefined = false;
      // CodeMirror needs createTextRange
      // https://discuss.codemirror.net/t/working-in-jsdom-or-node-js-natively/138/5
      (document.body: any).createTextRange = () => ({
        getBoundingClientRect: jest.fn(),
        getClientRects: () => ({}),
      });
    }
  });

  afterAll(() => {
    if (!isCreateTextRangeDefined) {
      delete (document.body: any).createTextRange;
    }
  });

  describe("getExpressionFromCoords", () => {
    it("returns null when location.line is greater than the lineCount", () => {
      const cm = CodeMirror(document.body, {
        value: "let Line1;\n" + "let Line2;\n",
        mode: "javascript",
      });

      const result = getExpressionFromCoords(cm, {
        line: 3,
        column: 1,
      });
      expect(result).toBeNull();
    });

    it("gets the expression using CodeMirror.getTokenAt", () => {
      const codemirrorMock = {
        lineCount: () => 100,
        getTokenAt: jest.fn(() => ({ start: 0, end: 0 })),
        doc: {
          getLine: () => "",
        },
      };
      getExpressionFromCoords(codemirrorMock, { line: 1, column: 1 });
      expect(codemirrorMock.getTokenAt).toHaveBeenCalled();
    });

    it("requests the correct line and column from codeMirror", () => {
      const codemirrorMock = {
        lineCount: () => 100,
        getTokenAt: jest.fn(() => ({ start: 0, end: 1 })),
        doc: {
          getLine: jest.fn(() => ""),
        },
      };
      getExpressionFromCoords(codemirrorMock, { line: 20, column: 5 });
      // getExpressionsFromCoords uses one based line indexing
      // CodeMirror uses zero based line indexing
      expect(codemirrorMock.getTokenAt).toHaveBeenCalledWith({
        line: 19,
        ch: 5,
      });
      expect(codemirrorMock.doc.getLine).toHaveBeenCalledWith(19);
    });

    it("when called with column 0 returns null", () => {
      const cm = CodeMirror(document.body, {
        value: "foo bar;\n",
        mode: "javascript",
      });

      const result = getExpressionFromCoords(cm, {
        line: 1,
        column: 0,
      });
      expect(result).toBeNull();
    });

    it("gets the expression when first token on the line", () => {
      const cm = CodeMirror(document.body, {
        value: "foo bar;\n",
        mode: "javascript",
      });

      const result = getExpressionFromCoords(cm, {
        line: 1,
        column: 1,
      });
      if (!result) {
        throw new Error("no result");
      }
      expect(result.expression).toEqual("foo");
      expect(result.location.start).toEqual({ line: 1, column: 0 });
      expect(result.location.end).toEqual({ line: 1, column: 3 });
    });

    it("includes previous tokens in the expression", () => {
      const cm = CodeMirror(document.body, {
        value: "foo.bar;\n",
        mode: "javascript",
      });

      const result = getExpressionFromCoords(cm, {
        line: 1,
        column: 5,
      });
      if (!result) {
        throw new Error("no result");
      }
      expect(result.expression).toEqual("foo.bar");
      expect(result.location.start).toEqual({ line: 1, column: 0 });
      expect(result.location.end).toEqual({ line: 1, column: 7 });
    });

    it("includes multiple previous tokens in the expression", () => {
      const cm = CodeMirror(document.body, {
        value: "foo.bar.baz;\n",
        mode: "javascript",
      });

      const result = getExpressionFromCoords(cm, {
        line: 1,
        column: 10,
      });
      if (!result) {
        throw new Error("no result");
      }
      expect(result.expression).toEqual("foo.bar.baz");
      expect(result.location.start).toEqual({ line: 1, column: 0 });
      expect(result.location.end).toEqual({ line: 1, column: 11 });
    });

    it("does not include tokens not part of the expression", () => {
      const cm = CodeMirror(document.body, {
        value: "foo bar.baz;\n",
        mode: "javascript",
      });

      const result = getExpressionFromCoords(cm, {
        line: 1,
        column: 10,
      });
      if (!result) {
        throw new Error("no result");
      }
      expect(result.expression).toEqual("bar.baz");
      expect(result.location.start).toEqual({ line: 1, column: 4 });
      expect(result.location.end).toEqual({ line: 1, column: 11 });
    });
  });
});
