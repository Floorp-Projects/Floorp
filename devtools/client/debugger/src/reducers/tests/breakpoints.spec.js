/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { initialBreakpointsState } from "../breakpoints";
import { getBreakpointsForSource } from "../../selectors/breakpoints";

import { makeMockBreakpoint, makeMockSource } from "../../utils/test-mockup";

function initializeStateWith(data) {
  const state = initialBreakpointsState();
  state.breakpoints = data;
  return state;
}

describe("Breakpoints Selectors", () => {
  it("gets a breakpoint for an original source", () => {
    const sourceId = "server1.conn1.child1/source1/originalSource";
    const source = makeMockSource(undefined, sourceId);
    const matchingBreakpoints = {
      id1: makeMockBreakpoint(source, 1),
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
    const sourceBreakpoints = getBreakpointsForSource({ breakpoints }, source);

    expect(sourceBreakpoints).toEqual(allBreakpoints);
    expect(sourceBreakpoints[0] === allBreakpoints[0]).toBe(true);
  });

  it("gets a breakpoint for a generated source", () => {
    const generatedSourceId = "random-source";
    const generatedSource = makeMockSource(undefined, generatedSourceId);
    const matchingBreakpoints = {
      id1: {
        ...makeMockBreakpoint(generatedSource, 1),
        location: { line: 1, source: { id: "original-source-id-1" } },
      },
    };

    const otherBreakpoints = {
      id2: {
        ...makeMockBreakpoint(makeMockSource(undefined, "not-this-source"), 1),
        location: { line: 1, source: { id: "original-source-id-2" } },
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
      generatedSource
    );

    expect(sourceBreakpoints).toEqual(allBreakpoints);
  });
});
