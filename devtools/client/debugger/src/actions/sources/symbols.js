/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSymbols } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";
import { loadSourceText } from "./loadSourceText";

import { memoizeableAction } from "../../utils/memoizableAction";
import { fulfilled } from "../../utils/async-value";

async function doSetSymbols(location, { dispatch, getState, parserWorker }) {
  await dispatch(loadSourceText(location.source, location.sourceActor));

  await dispatch({
    type: "SET_SYMBOLS",
    location,
    [PROMISE]: parserWorker.getSymbols(location.source.id),
  });
}

export const setSymbols = memoizeableAction("setSymbols", {
  getValue: (location, { getState, parserWorker }) => {
    if (!parserWorker.isLocationSupported(location)) {
      return fulfilled(null);
    }

    const symbols = getSymbols(getState(), location);
    if (!symbols) {
      return null;
    }

    return fulfilled(symbols);
  },
  createKey: location => location.source.id,
  action: (location, thunkArgs) => doSetSymbols(location, thunkArgs),
});

export function getFunctionSymbols(sourceId, maxResults) {
  return ({ parserWorker }) => {
    return parserWorker.getFunctionSymbols(sourceId, maxResults);
  };
}
