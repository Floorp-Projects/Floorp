/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { actions, createStore, makeSource } from "../../utils/test-head";
import { createLocation } from "../../utils/location";

import {
  getColumnBreakpoints,
  getFirstBreakpointPosition,
} from "../visibleColumnBreakpoints";
import {
  makeMockSource,
  makeMockSourceWithContent,
  makeMockBreakpoint,
} from "../../utils/test-mockup";

function pp(line, column) {
  return {
    location: { sourceId: "foo", line, column },
    generatedLocation: { sourceId: "foo", line, column },
  };
}

function defaultSource() {
  return makeMockSource(undefined, "foo");
}

function bp(line, column) {
  return makeMockBreakpoint(defaultSource(), line, column);
}

const source = makeMockSourceWithContent(
  undefined,
  "foo.js",
  undefined,
  `function foo() {
  console.log("hello");
}
console.log('bye');
`
);

describe("visible column breakpoints", () => {
  it("simple", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 },
    };
    const pausePoints = { 1: [pp(1, 1), pp(1, 5)], 3: [pp(3, 1)] };
    const breakpoints = [bp(1, 1), bp(4, 0), bp(4, 3)];

    const columnBps = getColumnBreakpoints(
      pausePoints,
      breakpoints,
      viewport,
      source,
      source.content
    );
    expect(columnBps).toMatchSnapshot();
  });

  it("ignores single breakpoints", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 },
    };
    const pausePoints = { 1: [pp(1, 1), pp(1, 3)], 2: [pp(2, 1)] };
    const breakpoints = [bp(1, 1)];
    const columnBps = getColumnBreakpoints(
      pausePoints,
      breakpoints,
      viewport,
      source,
      source.content
    );
    expect(columnBps).toMatchSnapshot();
  });

  it("only shows visible breakpoints", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 },
    };
    const pausePoints = { 1: [pp(1, 1), pp(1, 3)], 20: [pp(20, 1)] };
    const breakpoints = [bp(1, 1)];

    const columnBps = getColumnBreakpoints(
      pausePoints,
      breakpoints,
      viewport,
      source,
      source.content
    );
    expect(columnBps).toMatchSnapshot();
  });

  it("doesnt show breakpoints to the right", () => {
    const viewport = {
      start: { line: 1, column: 0 },
      end: { line: 10, column: 10 },
    };
    const pausePoints = { 1: [pp(1, 1), pp(1, 15)], 20: [pp(20, 1)] };
    const breakpoints = [bp(1, 1), bp(1, 15)];

    const columnBps = getColumnBreakpoints(
      pausePoints,
      breakpoints,
      viewport,
      source,
      source.content
    );
    expect(columnBps).toMatchSnapshot();
  });
});

describe("getFirstBreakpointPosition", () => {
  it("sorts the positions by column", async () => {
    const store = createStore();
    const { dispatch, getState } = store;

    const fooSource = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );

    dispatch({
      type: "ADD_BREAKPOINT_POSITIONS",
      positions: [pp(1, 5), pp(1, 3)],
      source: fooSource,
    });

    const position = getFirstBreakpointPosition(
      getState(),
      createLocation({
        line: 1,
        source: fooSource,
      })
    );

    if (!position) {
      throw new Error("There should be a position");
    }
    expect(position.location.column).toEqual(3);
  });
});
