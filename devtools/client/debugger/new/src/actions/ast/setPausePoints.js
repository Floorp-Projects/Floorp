/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSourceFromId } from "../../selectors";
import * as parser from "../../workers/parser";
import { isGenerated } from "../../utils/source";
import { mapPausePoints } from "../../utils/pause/pausePoints";
import { features } from "../../utils/prefs";

import type { SourceId } from "../types";
import type { ThunkArgs, Action } from "./types";

function compressPausePoints(pausePoints) {
  const compressed = {};
  for (const line in pausePoints) {
    compressed[line] = {};
    for (const col in pausePoints[line]) {
      const { types } = pausePoints[line][col];
      compressed[line][col] = (types.break ? 1 : 0) | (types.step ? 2 : 0);
    }
  }

  return compressed;
}

async function mapLocations(pausePoints, source, sourceMaps) {
  const sourceId = source.id;
  return mapPausePoints(pausePoints, async ({ types, location }) => {
    const generatedLocation = await sourceMaps.getGeneratedLocation(
      { ...location, sourceId },
      source
    );

    return { types, location, generatedLocation };
  });
}
export function setPausePoints(sourceId: SourceId) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);
    if (!features.pausePoints || !source || !source.text) {
      return;
    }

    if (source.isWasm) {
      return;
    }

    let pausePoints = await parser.getPausePoints(sourceId);

    if (features.columnBreakpoints) {
      pausePoints = await mapLocations(pausePoints, source, sourceMaps);
    }

    if (isGenerated(source)) {
      const compressed = compressPausePoints(pausePoints);
      await client.setPausePoints(sourceId, compressed);
    }

    dispatch(
      ({
        type: "SET_PAUSE_POINTS",
        sourceText: source.text || "",
        sourceId,
        pausePoints
      }: Action)
    );
  };
}
