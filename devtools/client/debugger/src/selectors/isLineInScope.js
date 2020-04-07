/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getInScopeLines } from "../reducers/ast";
import { getVisibleSelectedFrame } from "./pause";

import type { State } from "../reducers/types";

// Checks if a line is considered in scope
// We consider all lines in scope, if we do not have lines in scope.
export function isLineInScope(state: State, line: number): boolean {
  const frame = getVisibleSelectedFrame(state);
  if (!frame) {
    return false;
  }

  const lines = getInScopeLines(state, frame.location);
  if (!lines) {
    return true;
  }

  return lines.includes(line);
}
