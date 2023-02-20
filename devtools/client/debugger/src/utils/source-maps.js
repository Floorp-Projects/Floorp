/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isOriginalId } from "devtools/client/shared/source-map-loader/index";
import { getSource, getLocationSource } from "../selectors";

/**
 * For any location, return the matching generated location.
 * If this is already a generated location, returns the same location.
 *
 * In additional to `SourceMapLoader.getGeneratedLocation`,
 * this asserts that the related source is still registered in the reducer current state.
 *
 * @param {Object} location
 * @param {Object} thunkArgs
 *        Redux action thunk arguments
 * @param {Object}
 *        The matching generated location.
 */
export async function getGeneratedLocation(location, thunkArgs) {
  if (!isOriginalId(location.sourceId)) {
    return location;
  }

  const { sourceMapLoader, getState } = thunkArgs;
  const { line, sourceId, column } = await sourceMapLoader.getGeneratedLocation(
    location
  );

  const generatedSource = getSource(getState(), sourceId);
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

export async function getOriginalLocation(generatedLocation, sourceMapLoader) {
  if (isOriginalId(generatedLocation.sourceId)) {
    return location;
  }

  return sourceMapLoader.getOriginalLocation(generatedLocation);
}

export async function getMappedLocation(location, thunkArgs) {
  const { getState } = thunkArgs;
  const source = getLocationSource(getState(), location);

  if (!source) {
    throw new Error(`no source ${location.sourceId}`);
  }

  if (isOriginalId(location.sourceId)) {
    const generatedLocation = await getGeneratedLocation(location, thunkArgs);
    return { location, generatedLocation };
  }

  const generatedLocation = location;
  const { sourceMapLoader } = thunkArgs;
  const originalLocation = await sourceMapLoader.getOriginalLocation(
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
export async function getRelatedMapLocation(location, thunkArgs) {
  if (isOriginalId(location.sourceId)) {
    return getGeneratedLocation(location, thunkArgs);
  }

  const { sourceMapLoader } = thunkArgs;
  return sourceMapLoader.getOriginalLocation(location);
}
