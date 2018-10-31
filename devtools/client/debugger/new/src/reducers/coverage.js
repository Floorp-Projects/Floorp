/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Code coverage reducer
 * @module reducers/coverage
 */

import makeRecord from "../utils/makeRecord";
import * as I from "immutable";
import fromJS from "../utils/fromJS";

import type { Action } from "../actions/types";
import type { Record } from "../utils/makeRecord";

export type CoverageState = {
  coverageOn: boolean,
  hitCount: Object
};

export const createCoverageState = makeRecord(
  ({
    coverageOn: false,
    hitCount: I.Map()
  }: CoverageState)
);

function update(
  state: Record<CoverageState> = createCoverageState(),
  action: Action
): Record<CoverageState> {
  switch (action.type) {
    case "RECORD_COVERAGE":
      return state
        .mergeIn(["hitCount"], fromJS(action.value.coverage))
        .setIn(["coverageOn"], true);

    default: {
      return state;
    }
  }
}

// NOTE: we'd like to have the app state fully typed
// https://github.com/devtools-html/debugger.html/blob/master/src/reducers/sources.js#L179-L185
type OuterState = { coverage: Record<CoverageState> };

export function getHitCountForSource(state: OuterState, sourceId: ?string) {
  const hitCount = state.coverage.get("hitCount");
  return hitCount.get(sourceId);
}

export function getCoverageEnabled(state: OuterState) {
  return state.coverage.get("coverageOn");
}

export default update;
