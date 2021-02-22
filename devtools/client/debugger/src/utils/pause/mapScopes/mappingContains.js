/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { positionCmp } from "./positionCmp";

export function mappingContains(mapped, item) {
  return (
    positionCmp(item.start, mapped.start) >= 0 &&
    positionCmp(item.end, mapped.end) <= 0
  );
}
