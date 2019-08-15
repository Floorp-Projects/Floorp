/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { hasSymbols, getSymbols } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";
import { updateTab } from "../tabs";
import { loadSourceText } from "./loadSourceText";

import {
  memoizeableAction,
  type MemoizedAction,
} from "../../utils/memoizableAction";

import type { Source, Context } from "../../types";
import type { Symbols } from "../../reducers/types";

async function doSetSymbols(cx, source, { dispatch, getState, parser }) {
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

  return symbols;
}

type Args = { cx: Context, source: Source };

export const setSymbols: MemoizedAction<Args, ?Symbols> = memoizeableAction(
  "setSymbols",
  {
    hasValue: ({ source }, { getState }) =>
      source.isWasm || hasSymbols(getState(), source),
    getValue: ({ source }, { getState }) =>
      source.isWasm ? null : getSymbols(getState(), source),
    createKey: ({ source }) => source.id,
    action: ({ cx, source }, thunkArgs) => doSetSymbols(cx, source, thunkArgs),
  }
);
