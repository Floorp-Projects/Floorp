/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { QuickOpenAction } from "./types";

export function setQuickOpenQuery(query: string): QuickOpenAction {
  return {
    type: "SET_QUICK_OPEN_QUERY",
    query,
  };
}

export function openQuickOpen(query?: string): QuickOpenAction {
  if (query != null) {
    return { type: "OPEN_QUICK_OPEN", query };
  }
  return { type: "OPEN_QUICK_OPEN" };
}

export function closeQuickOpen(): QuickOpenAction {
  return { type: "CLOSE_QUICK_OPEN" };
}
