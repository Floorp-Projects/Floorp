/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { hasSymbols, getSymbols } from "../../selectors";

import { PROMISE } from "../utils/middleware/promise";
import { updateTab } from "../tabs";
import { loadSourceText } from "./loadSourceText";

import * as parser from "../../workers/parser";

import { isLoaded } from "../../utils/source";
import {
  memoizeableAction,
  type MemoizedAction
} from "../../utils/memoizableAction";

import type { Source } from "../../types";
import type { Symbols } from "../../reducers/types";

async function doSetSymbols(source, { dispatch, getState }) {
  const sourceId = source.id;

  if (!isLoaded(source)) {
    await dispatch(loadSourceText({ source }));
  }

  await dispatch({
    type: "SET_SYMBOLS",
    sourceId,
    [PROMISE]: parser.getSymbols(sourceId)
  });

  const symbols = getSymbols(getState(), source);
  if (symbols && symbols.framework) {
    dispatch(updateTab(source, symbols.framework));
  }

  return symbols;
}

type Args = { source: Source };

export const setSymbols: MemoizedAction<Args, ?Symbols> = memoizeableAction(
  "setSymbols",
  {
    exitEarly: ({ source }) => source.isWasm,
    hasValue: ({ source }, { getState }) => hasSymbols(getState(), source),
    getValue: ({ source }, { getState }) => getSymbols(getState(), source),
    createKey: ({ source }) => source.id,
    action: ({ source }, thunkArgs) => doSetSymbols(source, thunkArgs)
  }
);
