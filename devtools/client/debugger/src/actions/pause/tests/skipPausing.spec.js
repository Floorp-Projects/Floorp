/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { actions, selectors, createStore } from "../../../utils/test-head";

describe("sources - pretty print", () => {
  it("returns a pretty source for a minified file", async () => {
    const client = { setSkipPausing: jest.fn() };
    const { dispatch, getState } = createStore(client);

    await dispatch(actions.toggleSkipPausing());
    expect(selectors.getSkipPausing(getState())).toBe(true);

    await dispatch(actions.toggleSkipPausing());
    expect(selectors.getSkipPausing(getState())).toBe(false);
  });
});
