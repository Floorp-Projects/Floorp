/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedSource } from "../reducers/sources";
import { getPausePoints } from "../reducers/ast";
import { convertToList } from "../utils/pause/pausePoints";

export function getVisiblePausePoints(state) {
  const source = getSelectedSource(state);
  if (!source) {
    return null;
  }

  const pausePoints = getPausePoints(state, source.id);
  return convertToList(pausePoints);
}
