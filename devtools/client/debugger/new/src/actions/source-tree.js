/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

import type { Action, FocusItem, ThunkArgs } from "./types";
import type { Context } from "../types";

export function setExpandedState(thread: string, expanded: Set<string>) {
  return ({ dispatch, getState }: ThunkArgs) => {
    dispatch(
      ({
        type: "SET_EXPANDED_STATE",
        thread,
        expanded
      }: Action)
    );
  };
}

export function focusItem(cx: Context, item: FocusItem) {
  return ({ dispatch, getState }: ThunkArgs) => {
    dispatch({
      type: "SET_FOCUSED_SOURCE_ITEM",
      cx,
      item
    });
  };
}
