/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
import { getSourceFromId, getSourceThreads, getSymbols } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";
import { mapFrames } from "../pause";
import { updateTab } from "../tabs";

import * as parser from "../../workers/parser";

import { isLoaded } from "../../utils/source";

import type { SourceId } from "../../types";
import type { ThunkArgs } from "../types";

export function setSymbols(sourceId: SourceId) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);

    if (source.isWasm || getSymbols(getState(), source) || !isLoaded(source)) {
      return;
    }

    await dispatch({
      type: "SET_SYMBOLS",
      sourceId,
      [PROMISE]: parser.getSymbols(sourceId)
    });

    const threads = getSourceThreads(getState(), source);
    await Promise.all(threads.map(thread => dispatch(mapFrames(thread))));

    const symbols = getSymbols(getState(), source);
    if (symbols.framework) {
      dispatch(updateTab(source, symbols.framework));
    }
  };
}
