// @flow

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  makeSource,
  waitForState,
} from "../../../utils/test-head";
import { createSource } from "../../tests/helpers/threadFront";

describe("breakpointPositions", () => {
  it("fetches positions", async () => {
    const fooContent = createSource("foo", "");

    const store = createStore({
      getBreakpointPositions: async () => ({ "9": [1] }),
      getBreakableLines: async () => [],
      sourceContents: async () => fooContent,
    });

    const { dispatch, getState, cx } = store;
    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo"))
    );
    await dispatch(actions.loadSourceById(cx, source.id));

    dispatch(actions.setBreakpointPositions({ cx, sourceId: "foo", line: 9 }));

    await waitForState(store, state =>
      selectors.hasBreakpointPositions(state, "foo")
    );

    expect(
      selectors.getBreakpointPositionsForSource(getState(), "foo")
    ).toEqual({
      [9]: [
        {
          location: {
            line: 9,
            column: 1,
            sourceId: "foo",
            sourceUrl: "http://localhost:8000/examples/foo",
          },
          generatedLocation: {
            line: 9,
            column: 1,
            sourceId: "foo",
            sourceUrl: "http://localhost:8000/examples/foo",
          },
        },
      ],
    });
  });

  it("doesn't re-fetch positions", async () => {
    const fooContent = createSource("foo", "");

    let resolve = _ => {};
    let count = 0;
    const store = createStore({
      getBreakpointPositions: () =>
        new Promise(r => {
          count++;
          resolve = r;
        }),
      getBreakableLines: async () => [],
      sourceContents: async () => fooContent,
    });

    const { dispatch, getState, cx } = store;
    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo"))
    );
    await dispatch(actions.loadSourceById(cx, source.id));

    dispatch(actions.setBreakpointPositions({ cx, sourceId: "foo", line: 9 }));
    dispatch(actions.setBreakpointPositions({ cx, sourceId: "foo", line: 9 }));

    resolve({ "9": [1] });
    await waitForState(store, state =>
      selectors.hasBreakpointPositions(state, "foo")
    );

    expect(
      selectors.getBreakpointPositionsForSource(getState(), "foo")
    ).toEqual({
      [9]: [
        {
          location: {
            line: 9,
            column: 1,
            sourceId: "foo",
            sourceUrl: "http://localhost:8000/examples/foo",
          },
          generatedLocation: {
            line: 9,
            column: 1,
            sourceId: "foo",
            sourceUrl: "http://localhost:8000/examples/foo",
          },
        },
      ],
    });

    expect(count).toEqual(1);
  });
});
