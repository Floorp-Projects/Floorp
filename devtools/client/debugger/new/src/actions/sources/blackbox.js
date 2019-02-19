/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { isOriginalId, originalToGeneratedId } from "devtools-source-map";
import { recordEvent } from "../../utils/telemetry";
import { features } from "../../utils/prefs";
import { getSourceFromId } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";

import type { Source } from "../../types";
import type { ThunkArgs } from "../types";

async function blackboxActors(state, client, sourceId, isBlackBoxed, range?) {
  const source = getSourceFromId(state, sourceId);
  for (const sourceActor of source.actors) {
    await client.blackBox(sourceActor, isBlackBoxed, range);
  }
  return { isBlackBoxed: !isBlackBoxed };
}

export function toggleBlackBox(source: Source) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const { isBlackBoxed } = source;

    if (!isBlackBoxed) {
      recordEvent("blackbox");
    }

    let sourceId, range;
    if (features.originalBlackbox && isOriginalId(source.id)) {
      range = await sourceMaps.getFileGeneratedRange(source);
      sourceId = originalToGeneratedId(source.id);
    } else {
      sourceId = source.id;
    }

    return dispatch({
      type: "BLACKBOX",
      source,
      [PROMISE]: blackboxActors(
        getState(),
        client,
        sourceId,
        isBlackBoxed,
        range
      )
    });
  };
}
