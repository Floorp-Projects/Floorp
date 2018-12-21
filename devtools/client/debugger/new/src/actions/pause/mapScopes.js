/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getCurrentThread, getSource } from "../../selectors";
import { loadSourceText } from "../sources/loadSourceText";
import { PROMISE } from "../utils/middleware/promise";

import { features } from "../../utils/prefs";
import { log } from "../../utils/log";
import { isGenerated } from "../../utils/source";
import type { Frame, Scope } from "../../types";

import type { ThunkArgs } from "../types";

import { buildMappedScopes } from "../../utils/pause/mapScopes";

export function mapScopes(scopes: Promise<Scope>, frame: Frame) {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    const generatedSource = getSource(
      getState(),
      frame.generatedLocation.sourceId
    );

    const source = getSource(getState(), frame.location.sourceId);

    await dispatch({
      type: "MAP_SCOPES",
      thread: getCurrentThread(getState()),
      frame,
      [PROMISE]: (async function() {
        if (
          !features.mapScopes ||
          !source ||
          !generatedSource ||
          generatedSource.isWasm ||
          source.isPrettyPrinted ||
          isGenerated(source)
        ) {
          return null;
        }

        await dispatch(loadSourceText(source));

        try {
          return await buildMappedScopes(
            source,
            frame,
            await scopes,
            sourceMaps,
            client
          );
        } catch (e) {
          log(e);
          return null;
        }
      })()
    });
  };
}
