/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { actions, selectors, createStore } from "../../utils/test-head";
const { getExpandedState } = selectors;

describe("source tree", () => {
  it("should set the expanded state", () => {
    const { dispatch, getState } = createStore();
    const expandedState = new Set(["foo", "bar"]);

    expect(getExpandedState(getState(), "FakeThread")).toEqual(undefined);
    dispatch(actions.setExpandedState("FakeThread", expandedState));
    expect(getExpandedState(getState(), "FakeThread")).toEqual(expandedState);
  });
});
