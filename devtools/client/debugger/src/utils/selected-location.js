/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isOriginalId } from "devtools/client/shared/source-map-loader/index";

export function getSelectedLocation(mappedLocation, context) {
  if (!context) {
    return mappedLocation.location;
  }

  // context may be a source or a location
  const sourceId = context.source?.id || context.id;
  return isOriginalId(sourceId)
    ? mappedLocation.location
    : mappedLocation.generatedLocation;
}
