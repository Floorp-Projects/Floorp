/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSymbols, getSourceActorForSymbols } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";
import { loadSourceText } from "./loadSourceText";

import { memoizeableAction } from "../../utils/memoizableAction";
import { fulfilled } from "../../utils/async-value";

async function doSetSymbols(
  cx,
  source,
  sourceActor,
  { dispatch, getState, parserWorker }
) {
  const sourceId = source.id;

  await dispatch(loadSourceText(cx, source, sourceActor));

  await dispatch({
    type: "SET_SYMBOLS",
    cx,
    sourceId,
    // sourceActor can be null for original and pretty-printed sources
    sourceActorId: sourceActor ? sourceActor.actor : null,
    [PROMISE]: parserWorker.getSymbols(sourceId),
  });
}

export const setSymbols = memoizeableAction("setSymbols", {
  getValue: ({ source, sourceActor }, { getState }) => {
    if (source.isWasm) {
      return fulfilled(null);
    }

    const symbols = getSymbols(getState(), source);
    if (!symbols) {
      return null;
    }

    // Also check the spcific actor for the cached symbols
    if (
      sourceActor &&
      getSourceActorForSymbols(getState(), source) !== sourceActor.actor
    ) {
      return null;
    }

    return fulfilled(symbols);
  },
  createKey: ({ source }) => source.id,
  action: ({ cx, source, sourceActor }, thunkArgs) =>
    doSetSymbols(cx, source, sourceActor, thunkArgs),
});
