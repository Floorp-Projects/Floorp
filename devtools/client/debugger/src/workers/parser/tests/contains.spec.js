/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  containsPosition,
  containsLocation,
  nodeContainsPosition,
} from "../utils/contains";

function getTestLoc() {
  return {
    start: {
      line: 10,
      column: 2,
    },
    end: {
      line: 12,
      column: 10,
    },
  };
}

// AstPosition.column is typed as a number, but many parts of this test set it
// to undefined. Using zero instead causes test failures, and allowing it to be
// undefined causes many flow errors in code manipulating AstPosition.
// Fake a coercion of undefined to number as a workaround for now.
function undefinedColumn(): number {
  return (undefined: any);
}

function startPos(lineOffset, columnOffset) {
  const { start } = getTestLoc();
  return {
    line: start.line + lineOffset,
    column: start.column + columnOffset,
  };
}

function endPos(lineOffset, columnOffset) {
  const { end } = getTestLoc();
  return {
    line: end.line + lineOffset,
    column: end.column + columnOffset,
  };
}

function startLine(lineOffset = 0) {
  const { start } = getTestLoc();
  return {
    line: start.line + lineOffset,
    column: undefinedColumn(),
  };
}

function endLine(lineOffset = 0) {
  const { end } = getTestLoc();
  return {
    line: end.line + lineOffset,
    column: undefinedColumn(),
  };
}

function testContains(pos, bool) {
  const loc = getTestLoc();
  expect(containsPosition(loc, pos)).toEqual(bool);
}

function testContainsPosition(pos, bool) {
  const loc = getTestLoc();
  expect(nodeContainsPosition({ loc }, pos)).toEqual(bool);
}

describe("containsPosition", () => {
  describe("location and postion both with the column criteria", () => {
    it("should contain position within the location range", () =>
      testContains(startPos(1, 1), true));

    it("should not contain position out of the start line", () =>
      testContains(startPos(-1, 0), false));

    it("should not contain position out of the start column", () =>
      testContains(startPos(0, -1), false));

    it(`should contain position on the same start line and
       within the start column`, () => testContains(startPos(0, 1), true));

    it("should not contain position out of the end line", () =>
      testContains(endPos(1, 0), false));

    it("should not contain position out of the end column", () =>
      testContains(endPos(0, 1), false));

    // eslint-disable-next-line max-len
    it("should contain position on the same end line and within the end column", () =>
      testContains(endPos(0, -1), true));
  });

  describe("position without the column criterion", () => {
    it("should contain position on the same start line", () =>
      testContains(startLine(0), true));

    it("should contain position on the same end line", () =>
      testContains(endLine(0), true));
  });

  describe("location without the column criterion", () => {
    it("should contain position on the same start line", () => {
      const loc = getTestLoc();
      loc.start.column = undefinedColumn();
      const pos = {
        line: loc.start.line,
        column: 1,
      };
      expect(containsPosition(loc, pos)).toEqual(true);
    });

    it("should contain position on the same end line", () => {
      const loc = getTestLoc();
      loc.end.column = undefinedColumn();
      const pos = {
        line: loc.end.line,
        column: 1,
      };
      expect(containsPosition(loc, pos)).toEqual(true);
    });
  });

  describe("location and postion both without the column criterion", () => {
    it("should contain position on the same start line", () => {
      const loc = getTestLoc();
      loc.start.column = undefinedColumn();
      const pos = startLine();
      expect(containsPosition(loc, pos)).toEqual(true);
    });

    it("should contain position on the same end line", () => {
      const loc = getTestLoc();
      loc.end.column = undefinedColumn();
      const pos = endLine();
      expect(containsPosition(loc, pos)).toEqual(true);
    });
  });
});

describe("containsLocation", () => {
  describe("locations both with the column criteria", () => {
    it("should contian location within the range", () => {
      const locA = getTestLoc();
      const locB = {
        start: startPos(1, 1),
        end: endPos(-1, -1),
      };
      expect(containsLocation(locA, locB)).toEqual(true);
    });

    it("should not contian location out of the start line", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locB.start.line--;
      expect(containsLocation(locA, locB)).toEqual(false);
    });

    it("should not contian location out of the start column", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locB.start.column--;
      expect(containsLocation(locA, locB)).toEqual(false);
    });

    it("should not contian location out of the end line", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locB.end.line++;
      expect(containsLocation(locA, locB)).toEqual(false);
    });

    it("should not contian location out of the end column", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locB.end.column++;
      expect(containsLocation(locA, locB)).toEqual(false);
    });

    it(`should contain location on the same start line and
        within the start column`, () => {
      const locA = getTestLoc();
      const locB = {
        start: startPos(0, 1),
        end: endPos(-1, -1),
      };
      expect(containsLocation(locA, locB)).toEqual(true);
    });

    it(`should contain location on the same end line and
        within the end column`, () => {
      const locA = getTestLoc();
      const locB = {
        start: startPos(1, 1),
        end: endPos(0, -1),
      };
      expect(containsLocation(locA, locB)).toEqual(true);
    });
  });

  describe("location A without the column criterion", () => {
    it("should contain location on the same start line", () => {
      const locA = getTestLoc();
      locA.start.column = undefinedColumn();
      const locB = getTestLoc();
      expect(containsLocation(locA, locB)).toEqual(true);
    });

    it("should contain location on the same end line", () => {
      const locA = getTestLoc();
      locA.end.column = undefinedColumn();
      const locB = getTestLoc();
      expect(containsLocation(locA, locB)).toEqual(true);
    });
  });

  describe("location B without the column criterion", () => {
    it("should contain location on the same start line", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locB.start.column = undefinedColumn();
      expect(containsLocation(locA, locB)).toEqual(true);
    });

    it("should contain location on the same end line", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locB.end.column = undefinedColumn();
      expect(containsLocation(locA, locB)).toEqual(true);
    });
  });

  describe("locations both without the column criteria", () => {
    it("should contain location on the same start line", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locA.start.column = undefinedColumn();
      locB.start.column = undefinedColumn();
      expect(containsLocation(locA, locB)).toEqual(true);
    });

    it("should contain location on the same end line", () => {
      const locA = getTestLoc();
      const locB = getTestLoc();
      locA.end.column = undefinedColumn();
      locB.end.column = undefinedColumn();
      expect(containsLocation(locA, locB)).toEqual(true);
    });
  });
});

describe("nodeContainsPosition", () => {
  describe("node and position both with the column criteria", () => {
    it("should contian position within the range", () =>
      testContainsPosition(startPos(1, 1), true));

    it("should not contian position out of the start line", () =>
      testContainsPosition(startPos(-1, 0), false));

    it("should not contian position out of the start column", () =>
      testContainsPosition(startPos(0, -1), false));

    it("should not contian position out of the end line", () =>
      testContainsPosition(endPos(1, 0), false));

    it("should not contian position out of the end column", () =>
      testContainsPosition(endPos(0, 1), false));

    it(`should contain position on the same start line and
        within the start column`, () =>
      testContainsPosition(startPos(0, 1), true));

    it(`should contain position on the same end line and
        within the end column`, () =>
      testContainsPosition(endPos(0, -1), true));
  });

  describe("node without the column criterion", () => {
    it("should contain position on the same start line", () => {
      const loc = getTestLoc();
      loc.start.column = undefinedColumn();
      const pos = startPos(0, -1);
      expect(nodeContainsPosition({ loc }, pos)).toEqual(true);
    });

    it("should contain position on the same end line", () => {
      const loc = getTestLoc();
      loc.end.column = undefinedColumn();
      const pos = startPos(0, 1);
      expect(nodeContainsPosition({ loc }, pos)).toEqual(true);
    });
  });

  describe("position without the column criterion", () => {
    it("should contain position on the same start line", () =>
      testContainsPosition(startLine(), true));

    it("should contain position on the same end line", () =>
      testContainsPosition(endLine(), true));
  });

  describe("node and position both without the column criteria", () => {
    it("should contain position on the same start line", () => {
      const loc = getTestLoc();
      loc.start.column = undefinedColumn();
      const pos = startLine();
      expect(nodeContainsPosition({ loc }, pos)).toEqual(true);
    });

    it("should contain position on the same end line", () => {
      const loc = getTestLoc();
      loc.end.column = undefinedColumn();
      const pos = endLine();
      expect(nodeContainsPosition({ loc }, pos)).toEqual(true);
    });
  });
});
