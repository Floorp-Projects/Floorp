/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { initialState } from "../../reducers/index";
import { getDisplayedSources } from "../index";

describe("ensure selectors are memoized", () => {
  it("ensure that getDisplayedSources is cached", () => {
    const state = initialState();
    expect(getDisplayedSources(state)).toEqual(getDisplayedSources(state));
  });
});
