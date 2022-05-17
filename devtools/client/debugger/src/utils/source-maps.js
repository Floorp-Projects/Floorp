/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isOriginalId } from "devtools-source-map";
import { getSource, getLocationSource } from "../selectors";

export async function getGeneratedLocation(
  state,
  source,
  location,
  sourceMaps
) {
  if (!isOriginalId(location.sourceId)) {
    return location;
  }

  const { line, sourceId, column } = await sourceMaps.getGeneratedLocation(
    location
  );

  const generatedSource = getSource(state, sourceId);
  if (!generatedSource) {
    throw new Error(`Could not find generated source ${sourceId}`);
  }

  return {
    line,
    sourceId,
    column: column === 0 ? undefined : column,
    sourceUrl: generatedSource.url,
  };
}

export async function getOriginalLocation(generatedLocation, sourceMaps) {
  if (isOriginalId(generatedLocation.sourceId)) {
    return location;
  }

  return sourceMaps.getOriginalLocation(generatedLocation);
}

export async function getMappedLocation(state, sourceMaps, location) {
  const source = getLocationSource(state, location);

  if (!source) {
    throw new Error(`no source ${location.sourceId}`);
  }

  if (isOriginalId(location.sourceId)) {
    const generatedLocation = await getGeneratedLocation(
      state,
      source,
      location,
      sourceMaps
    );
    return { location, generatedLocation };
  }

  const generatedLocation = location;
  const originalLocation = await sourceMaps.getOriginalLocation(
    generatedLocation
  );

  return { location: originalLocation, generatedLocation };
}

/**
 * Gets the "mapped location".
 *
 * If the passed location is on a generated source, it gets the
 * related location in the original source.
 * If the passed location is on an original source, it gets the
 * related location in the generated source.
 */
export async function getRelatedMapLocation(state, sourceMaps, location) {
  const source = getLocationSource(state, location);

  if (!source) {
    return location;
  }

  if (isOriginalId(location.sourceId)) {
    return getGeneratedLocation(state, source, location, sourceMaps);
  }

  return sourceMaps.getOriginalLocation(location);
}
