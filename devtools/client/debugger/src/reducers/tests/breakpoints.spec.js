/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
declare var describe: (name: string, func: () => void) => void;
declare var it: (desc: string, func: () => void) => void;
declare var expect: (value: any) => any;

import {
  getBreakpointsForSource,
  initialBreakpointsState,
} from "../breakpoints";

import { makeMockBreakpoint, makeMockSource } from "../../utils/test-mockup";

function initializeStateWith(data) {
  const state = initialBreakpointsState();
  state.breakpoints = data;
  return state;
}

describe("Breakpoints Selectors", () => {
  it("it gets a breakpoint for an original source", () => {
    const sourceId = "server1.conn1.child1/source1/originalSource";
    const matchingBreakpoints = {
      id1: makeMockBreakpoint(makeMockSource(undefined, sourceId), 1),
    };

    const otherBreakpoints = {
      id2: makeMockBreakpoint(makeMockSource(undefined, "not-this-source"), 1),
    };

    const data = {
      ...matchingBreakpoints,
      ...otherBreakpoints,
    };

    const breakpoints = initializeStateWith(data);
    const allBreakpoints = Object.values(matchingBreakpoints);
    const sourceBreakpoints = getBreakpointsForSource(
      { breakpoints },
      sourceId
    );

    expect(sourceBreakpoints).toEqual(allBreakpoints);
    expect(sourceBreakpoints[0] === allBreakpoints[0]).toBe(true);
  });

  it("it gets a breakpoint for a generated source", () => {
    const generatedSourceId = "random-source";
    const matchingBreakpoints = {
      id1: {
        ...makeMockBreakpoint(makeMockSource(undefined, generatedSourceId), 1),
        location: { line: 1, sourceId: "original-source-id-1" },
      },
    };

    const otherBreakpoints = {
      id2: {
        ...makeMockBreakpoint(makeMockSource(undefined, "not-this-source"), 1),
        location: { line: 1, sourceId: "original-source-id-2" },
      },
    };

    const data = {
      ...matchingBreakpoints,
      ...otherBreakpoints,
    };

    const breakpoints = initializeStateWith(data);

    const allBreakpoints = Object.values(matchingBreakpoints);
    const sourceBreakpoints = getBreakpointsForSource(
      { breakpoints },
      generatedSourceId
    );

    expect(sourceBreakpoints).toEqual(allBreakpoints);
  });
});
