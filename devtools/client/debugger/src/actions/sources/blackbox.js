/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { isOriginalId, originalToGeneratedId } from "devtools-source-map";
import { recordEvent } from "../../utils/telemetry";
import { features } from "../../utils/prefs";
import { getSourceActorsForSource } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";

async function blackboxActors(state, client, sourceId, isBlackBoxed, range) {
  for (const actor of getSourceActorsForSource(state, sourceId)) {
    await client.blackBox(actor, isBlackBoxed, range);
  }
  return { isBlackBoxed: !isBlackBoxed };
}

async function getSourceId(source, sourceMaps) {
  let sourceId = source.id,
    range;
  if (features.originalBlackbox && isOriginalId(source.id)) {
    range = await sourceMaps.getFileGeneratedRange(source.id);
    sourceId = originalToGeneratedId(source.id);
  }
  return { sourceId, range };
}

export function toggleBlackBox(cx, source) {
  return async ({ dispatch, getState, client, sourceMaps }) => {
    const { isBlackBoxed } = source;

    if (!isBlackBoxed) {
      recordEvent("blackbox");
    }

    const { sourceId, range } = await getSourceId(source, sourceMaps);

    return dispatch({
      type: "BLACKBOX",
      cx,
      source,
      [PROMISE]: blackboxActors(
        getState(),
        client,
        sourceId,
        isBlackBoxed,
        range
      ),
    });
  };
}

export function blackBoxSources(cx, sourcesToBlackBox, shouldBlackBox) {
  return async ({ dispatch, getState, client, sourceMaps }) => {
    const state = getState();
    const sources = sourcesToBlackBox.filter(
      source => source.isBlackBoxed !== shouldBlackBox
    );

    if (shouldBlackBox) {
      recordEvent("blackbox");
    }

    const promises = [
      ...sources.map(async source => {
        const { sourceId, range } = await getSourceId(source, sourceMaps);

        return getSourceActorsForSource(state, sourceId).map(actor =>
          client.blackBox(actor, source.isBlackBoxed, range)
        );
      }),
    ];

    return dispatch({
      type: "BLACKBOX_SOURCES",
      cx,
      shouldBlackBox,
      [PROMISE]: Promise.all(promises).then(() => ({ sources })),
    });
  };
}
