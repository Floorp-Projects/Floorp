/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { isOriginalId } from "devtools-source-map";
import { recordEvent } from "../../utils/telemetry";
import { features } from "../../utils/prefs";

import { PROMISE } from "../utils/middleware/promise";
import type { Source } from "../../types";
import type { ThunkArgs } from "../types";

export function toggleBlackBox(source: Source) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const { isBlackBoxed, id } = source;

    if (!isBlackBoxed) {
      recordEvent("blackbox");
    }

    let promise;
    if (features.originalBlackbox && isOriginalId(id)) {
      promise = Promise.resolve({isBlackBoxed: !isBlackBoxed})
    } else {
      promise = client.blackBox(id, isBlackBoxed)
    }

    return dispatch({
      type: "BLACKBOX",
      source,
      [PROMISE]: promise
    });
  };
}
