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
import { getSourceActorsForSource } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";

import type { Source, Context, SourceId } from "../../types";
import type { ThunkArgs } from "../types";

async function blackboxActors(
  state,
  client,
  sourceId: SourceId,
  isBlackBoxed: boolean,
  range?
) {
  for (const actor of getSourceActorsForSource(state, sourceId)) {
    await client.blackBox(actor, isBlackBoxed, range);
  }
  return { isBlackBoxed: !isBlackBoxed };
}

async function getSourceId(source: Source, sourceMaps) {
  let sourceId, range;
  if (features.originalBlackbox && isOriginalId(source.id)) {
    range = await sourceMaps.getFileGeneratedRange(source.id);
    sourceId = originalToGeneratedId(source.id);
  } else {
    sourceId = source.id;
  }
  return { sourceId, range };
}

export function toggleBlackBox(cx: Context, source: Source) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
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

export function blackBoxSources(
  cx: Context,
  sourcesToBlackBox: Source[],
  shouldBlackBox: boolean
) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
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
