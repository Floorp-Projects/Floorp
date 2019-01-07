/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  createStore,
  selectors,
  actions,
  makeSource,
  waitForState
} from "../../../utils/test-head";

import { generateBreakpoint } from "../../tests/helpers/breakpoints.js";

import { simpleMockThreadClient } from "../../tests/helpers/threadClient.js";

describe("toggleBreakpointsAtLine", () => {
  it("removes all breakpoints on a given line", async () => {
    const store = createStore(simpleMockThreadClient);
    const { dispatch } = store;

    const source = makeSource("foo.js");
    await dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText(source));

    await Promise.all([
      dispatch(
        actions.addBreakpoint(generateBreakpoint("foo.js", 5, 1).location)
      ),
      dispatch(
        actions.addBreakpoint(generateBreakpoint("foo.js", 5, 2).location)
      ),
      dispatch(
        actions.addBreakpoint(generateBreakpoint("foo.js", 5, 3).location)
      )
    ]);

    await dispatch(actions.selectLocation({ sourceId: "foo.js" }));

    await waitForState(store, state => selectors.hasSymbols(state, source));

    await dispatch(actions.toggleBreakpointsAtLine(5));
    await waitForState(
      store,
      state => selectors.getBreakpointCount(state) === 0
    );
  });

  it("removes all breakpoints on an empty line", async () => {
    const store = createStore(simpleMockThreadClient);
    const { dispatch } = store;

    const source = makeSource("foo.js");
    await dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText(makeSource("foo.js")));

    await dispatch(actions.addBreakpoint({ sourceId: source.id, line: 3 }));
    await dispatch(actions.selectLocation({ sourceId: "foo.js" }));

    await waitForState(store, state =>
      selectors.hasPausePoints(state, source.id)
    );

    await dispatch(actions.toggleBreakpointsAtLine(3));

    await waitForState(
      store,
      state => selectors.getBreakpointCount(state) === 0
    );
  });
});
