/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSourceWithContent, getSelectedLocation } from "../selectors";

import { setInScopeLines } from "./ast/setInScopeLines";

import type { Context } from "../types";
import type { ThunkArgs, Action } from "./types";

export function setOutOfScopeLocations(cx: Context) {
  return async ({ dispatch, getState, parser }: ThunkArgs) => {
    const location = getSelectedLocation(getState());
    if (!location) {
      return;
    }

    const { source, content } = getSourceWithContent(
      getState(),
      location.sourceId
    );

    if (!content) {
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
        locations,
      }: Action)
    );
    dispatch(setInScopeLines(cx));
  };
}
