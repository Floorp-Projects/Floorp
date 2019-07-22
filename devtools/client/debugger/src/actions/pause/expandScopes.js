/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getScopeItemPath } from "../../utils/pause/scopes/utils";
import type { ThunkArgs } from "../types";
import type { ThreadContext } from "../../types";

export function setExpandedScope(
  cx: ThreadContext,
  item: Object,
  expanded: boolean
) {
  return function({ dispatch, getState }: ThunkArgs) {
    return dispatch({
      type: "SET_EXPANDED_SCOPE",
      cx,
      thread: cx.thread,
      path: getScopeItemPath(item),
      expanded,
    });
  };
}
