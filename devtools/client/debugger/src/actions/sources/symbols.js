/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSymbols } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";
import { updateTab } from "../tabs";
import { loadSourceText } from "./loadSourceText";

import {
  memoizeableAction,
  type MemoizedAction,
} from "../../utils/memoizableAction";
import { fulfilled } from "../../utils/async-value";

import type { ThunkArgs } from "../../actions/types";
import type { Source, Context } from "../../types";
import type { Symbols } from "../../reducers/types";

async function doSetSymbols(
  cx,
  source: Source,
  { dispatch, getState, parser }: ThunkArgs
) {
  const sourceId = source.id;

  await dispatch(loadSourceText({ cx, source }));

  await dispatch({
    type: "SET_SYMBOLS",
    cx,
    sourceId,
    [PROMISE]: parser.getSymbols(sourceId),
  });

  const symbols = getSymbols(getState(), source);
  if (symbols && symbols.framework) {
    dispatch(updateTab(source, symbols.framework));
  }
}

export const setSymbols: MemoizedAction<
  {| cx: Context, source: Source |},
  ?Symbols
> = memoizeableAction("setSymbols", {
  getValue: ({ source }, { getState }) => {
    if (source.isWasm) {
      return fulfilled(null);
    }

    const symbols = getSymbols(getState(), source);
    if (!symbols || symbols.loading) {
      return null;
    }

    return fulfilled(symbols);
  },
  createKey: ({ source }) => source.id,
  action: ({ cx, source }, thunkArgs) => doSetSymbols(cx, source, thunkArgs),
});
