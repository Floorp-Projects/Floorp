/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  createStore,
  getTelemetryEvents,
} from "../../../utils/test-head";

import {
  getShouldPauseOnExceptions,
  getShouldPauseOnCaughtExceptions,
} from "../../../reducers/pause";

describe("pauseOnExceptions", () => {
  it("should track telemetry for pauseOnException changes", async () => {
    const { dispatch, getState } = createStore({ pauseOnExceptions: () => {} });
    dispatch(actions.pauseOnExceptions(true, false));
    expect(getTelemetryEvents("pause_on_exceptions")).toMatchSnapshot();
    expect(getShouldPauseOnExceptions(getState())).toBe(true);
    expect(getShouldPauseOnCaughtExceptions(getState())).toBe(false);
  });
});
