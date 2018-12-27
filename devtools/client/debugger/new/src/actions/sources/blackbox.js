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

import { PROMISE } from "../utils/middleware/promise";

import type { Source } from "../../types";
import type { ThunkArgs } from "../types";

export function toggleBlackBox(source: Source) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const { isBlackBoxed } = source;

    if (!isBlackBoxed) {
      recordEvent("blackbox");
    }

    let promise;

    if (features.originalBlackbox && isOriginalId(source.id)) {
      const range = await sourceMaps.getFileGeneratedRange(source);
      const generatedId = originalToGeneratedId(source.id);
      promise = client.blackBox(generatedId, isBlackBoxed, range);
    } else {
      promise = client.blackBox(source.id, isBlackBoxed);
    }

    return dispatch({
      type: "BLACKBOX",
      source,
      [PROMISE]: promise
    });
  };
}
