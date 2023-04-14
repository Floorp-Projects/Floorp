/* eslint max-nested-callbacks: ["error", 6] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import readFixture from "../../tests/helpers/readFixture";

import { makeMockFrame, makeMockSource } from "../../../utils/test-mockup";
import {
  createStore,
  selectors,
  actions,
  makeSource,
  waitForState,
} from "../../../utils/test-head";
import { createLocation } from "../../../utils/location";

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
  getFrames: async () => [],
  getSourceActorBreakpointPositions: async () => ({}),
  getSourceActorBreakableLines: async () => [],
};

describe("getInScopeLine", () => {
  it("with selected line", async () => {
    const client = { ...mockCommandClient };
    const store = createStore(client);
    const { dispatch, getState } = store;
    const source = makeMockSource("scopes.js", "scopes.js");
    const frame = makeMockFrame("scopes-4", source);
    client.getFrames = async () => [frame];

    const baseSource = await dispatch(
      actions.newGeneratedSource(makeSource("scopes.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      baseSource.id
    );

    await dispatch(
      actions.selectLocation(
        selectors.getContext(getState()),
        createLocation({
          source: baseSource,
          sourceActor,
          line: 5,
        })
      )
    );

    await dispatch(
      actions.paused({
        thread: "FakeThread",
        why: { type: "debuggerStatement" },
        frame,
      })
    );
    await dispatch(actions.setInScopeLines(selectors.getContext(getState())));

    await waitForState(store, state => getInScopeLines(state, frame.location));

    const lines = getInScopeLines(getState(), frame.location);

    expect(lines).toMatchSnapshot();
  });
});
