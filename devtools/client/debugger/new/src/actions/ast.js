/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSourceFromId, getSelectedLocation } from "../selectors";

import { setInScopeLines } from "./ast/setInScopeLines";

import * as parser from "../workers/parser";

import { isLoaded } from "../utils/source";

import type { Context } from "../types";
import type { ThunkArgs, Action } from "./types";

export function setOutOfScopeLocations(cx: Context) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const location = getSelectedLocation(getState());
    if (!location) {
      return;
    }

    const source = getSourceFromId(getState(), location.sourceId);

    if (!isLoaded(source)) {
      return;
    }

    let locations = null;
    if (location.line && source && !source.isWasm) {
      locations = await parser.findOutOfScopeLocations(
        source.id,
        ((location: any): parser.AstPosition)
      );
    }

    dispatch(
      ({
        type: "OUT_OF_SCOPE_LOCATIONS",
        cx,
        locations
      }: Action)
    );
    dispatch(setInScopeLines(cx));
  };
}
