/* eslint max-nested-callbacks: ["error", 6] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import readFixture from "../../tests/helpers/readFixture";

import { makeMockFrame, makeMockSource } from "../../../utils/test-mockup";
import {
  createStore,
  selectors,
  actions,
  makeSource,
  waitForState,
} from "../../../utils/test-head";

const { getInScopeLines } = selectors;

const sourceTexts = {
  "scopes.js": readFixture("scopes.js"),
};

const mockCommandClient = {
  sourceContents: async ({ source }) => ({
    source: sourceTexts[source],
    contentType: "text/javascript",
  }),
  evaluateExpressions: async () => {},
  getFrameScopes: async () => {},
  getSourceActorBreakpointPositions: async () => ({}),
  getSourceActorBreakableLines: async () => [],
};

describe("getInScopeLine", () => {
  it("with selected line", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, getState } = store;
    const source = makeMockSource("scopes.js", "scopes.js");

    await dispatch(actions.newGeneratedSource(makeSource("scopes.js")));

    await dispatch(
      actions.selectLocation(selectors.getContext(getState()), {
        sourceId: "scopes.js",
        line: 5,
      })
    );

    const frame = makeMockFrame("scopes-4", source);
    await dispatch(
      actions.paused({
        thread: "FakeThread",
        why: { type: "debuggerStatement" },
        frame,
        frames: [frame],
      })
    );
    await dispatch(actions.setInScopeLines(selectors.getContext(getState())));

    await waitForState(store, state => getInScopeLines(state, frame.location));

    const lines = getInScopeLines(getState(), frame.location);

    expect(lines).toMatchSnapshot();
  });
});
