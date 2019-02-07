/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSelectedSource } from "../reducers/sources";
import { getPausePoints } from "../reducers/ast";

import type { State } from "../reducers/types";

export function getVisiblePausePoints(state: State) {
  const source = getSelectedSource(state);
  if (!source) {
    return null;
  }

  return getPausePoints(state, source.id);
}
