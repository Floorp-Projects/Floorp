/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createStore, selectors, actions } from "../../utils/test-head";

jest.mock("../../utils/editor");

const { getActiveSearch } = selectors;

const threadFront = {
  sourceContents: async () => ({
    source: "function foo1() {\n  const foo = 5; return foo;\n}",
    contentType: "text/javascript",
  }),
  getSourceActorBreakpointPositions: async () => ({}),
  getSourceActorBreakableLines: async () => [],
};

describe("navigation", () => {
  it("navigation removes activeSearch 'file' value", async () => {
    const { dispatch, getState } = createStore(threadFront);
    dispatch(actions.setActiveSearch("file"));
    expect(getActiveSearch(getState())).toBe("file");

    await dispatch(actions.willNavigate("will-navigate"));
    expect(getActiveSearch(getState())).toBe(null);
  });
});
