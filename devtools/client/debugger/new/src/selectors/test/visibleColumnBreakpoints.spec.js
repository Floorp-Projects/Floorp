/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getColumnBreakpoints } from "../visibleColumnBreakpoints";

function pp(line, column) {
  return {
    location: { line, column },
    generatedLocation: { line, column },
    types: { break: true }
  };
}

function bp(line, column) {
  return { location: { line, column, sourceId: "foo" } };
}

describe("visible column breakpoints", () => {
  it("simple", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 }
    };
    const pausePoints = [pp(1, 1), pp(1, 5), pp(3, 1)];
    const breakpoints = [bp(1, 1), bp(4, 0), bp(4, 3)];

    const columnBps = getColumnBreakpoints(pausePoints, breakpoints, viewport);
    expect(columnBps).toMatchSnapshot();
  });

  it("duplicate generated locations", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 }
    };
    const pausePoints = [pp(1, 1), pp(1, 1), pp(1, 3)];
    const breakpoints = [bp(1, 1)];

    const columnBps = getColumnBreakpoints(pausePoints, breakpoints, viewport);
    expect(columnBps).toMatchSnapshot();
  });

  it("ignores single breakpoints", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 }
    };
    const pausePoints = [pp(1, 1), pp(1, 3, pp(2, 1))];
    const breakpoints = [bp(1, 1)];

    const columnBps = getColumnBreakpoints(pausePoints, breakpoints, viewport);
    expect(columnBps).toMatchSnapshot();
  });

  it("only shows visible breakpoints", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 }
    };
    const pausePoints = [pp(1, 1), pp(1, 3), pp(20, 1)];
    const breakpoints = [bp(1, 1)];

    const columnBps = getColumnBreakpoints(pausePoints, breakpoints, viewport);
    expect(columnBps).toMatchSnapshot();
  });
});
