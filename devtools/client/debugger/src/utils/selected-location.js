/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function getSelectedLocation(mappedLocation, context) {
  if (!context) {
    return mappedLocation.location;
  }

  // `context` may be a location or directly a source object.
  const source = context.source || context;
  return source.isOriginal
    ? mappedLocation.location
    : mappedLocation.generatedLocation;
}
