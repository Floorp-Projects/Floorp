/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isOriginalId } from "devtools-source-map";
import type { SourceLocation, MappedLocation, Source } from "../types";

export function getSelectedLocation(
  mappedLocation: MappedLocation,
  context: ?(Source | SourceLocation)
): SourceLocation {
  if (!context) {
    return mappedLocation.location;
  }

  // $FlowIgnore
  const sourceId = context.sourceId || context.id;
  return isOriginalId(sourceId)
    ? mappedLocation.location
    : mappedLocation.generatedLocation;
}
