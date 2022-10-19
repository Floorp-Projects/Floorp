/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSymbols } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";
import { loadSourceText } from "./loadSourceText";

import { memoizeableAction } from "../../utils/memoizableAction";
import { fulfilled } from "../../utils/async-value";

async function doSetSymbols(cx, source, { dispatch, getState, parser }) {
  const sourceId = source.id;

  await dispatch(loadSourceText({ cx, source }));

  await dispatch({
    type: "SET_SYMBOLS",
    cx,
    sourceId,
    [PROMISE]: parser.getSymbols(sourceId),
  });
}

export const setSymbols = memoizeableAction("setSymbols", {
  getValue: ({ source }, { getState }) => {
    if (source.isWasm) {
      return fulfilled(null);
    }

    const symbols = getSymbols(getState(), source);
    if (!symbols) {
      return null;
    }

    return fulfilled(symbols);
  },
  createKey: ({ source }) => source.id,
  action: ({ cx, source }, thunkArgs) => doSetSymbols(cx, source, thunkArgs),
});
