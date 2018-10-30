/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { recordEvent } from "../../utils/telemetry";

import { PROMISE } from "../utils/middleware/promise";
import type { Source } from "../../types";
import type { ThunkArgs } from "../types";

export function toggleBlackBox(source: Source) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const { isBlackBoxed, id } = source;

    if (!isBlackBoxed) {
      recordEvent("blackbox");
    }

    return dispatch({
      type: "BLACKBOX",
      source,
      [PROMISE]: client.blackBox(id, isBlackBoxed)
    });
  };
}
