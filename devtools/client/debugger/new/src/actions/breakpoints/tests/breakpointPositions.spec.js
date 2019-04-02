// @flow

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  makeSource,
  waitForState
} from "../../../utils/test-head";

describe("breakpointPositions", () => {
  it("fetches positions", async () => {
    const store = createStore({
      getBreakpointPositions: async () => ({ "9": [1] })
    });

    const { dispatch, getState } = store;
    await dispatch(actions.newSource(makeSource("foo")));

    dispatch(actions.setBreakpointPositions({ sourceId: "foo" }));

    await waitForState(store, state =>
      selectors.hasBreakpointPositions(state, "foo")
    );

    expect(
      selectors.getBreakpointPositionsForSource(getState(), "foo")
    ).toEqual([
      {
        location: {
          line: 9,
          column: 1,
          sourceId: "foo",
          sourceUrl: "http://localhost:8000/examples/foo"
        },
        generatedLocation: {
          line: 9,
          column: 1,
          sourceId: "foo",
          sourceUrl: "http://localhost:8000/examples/foo"
        }
      }
    ]);
  });

  it("doesn't re-fetch positions", async () => {
    let resolve = _ => {};
    let count = 0;
    const store = createStore({
      getBreakpointPositions: () =>
        new Promise(r => {
          count++;
          resolve = r;
        })
    });

    const { dispatch, getState } = store;
    await dispatch(actions.newSource(makeSource("foo")));

    dispatch(actions.setBreakpointPositions({ sourceId: "foo" }));
    dispatch(actions.setBreakpointPositions({ sourceId: "foo" }));

    resolve({ "9": [1] });
    await waitForState(store, state =>
      selectors.hasBreakpointPositions(state, "foo")
    );

    expect(
      selectors.getBreakpointPositionsForSource(getState(), "foo")
    ).toEqual([
      {
        location: {
          line: 9,
          column: 1,
          sourceId: "foo",
          sourceUrl: "http://localhost:8000/examples/foo"
        },
        generatedLocation: {
          line: 9,
          column: 1,
          sourceId: "foo",
          sourceUrl: "http://localhost:8000/examples/foo"
        }
      }
    ]);

    expect(count).toEqual(1);
  });
});
