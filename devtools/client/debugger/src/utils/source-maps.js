/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import SourceMaps, { isOriginalId } from "devtools-source-map";
import { getSource } from "../selectors";

import type { SourceLocation, MappedLocation, Source } from "../types";

export async function getGeneratedLocation(
  state: Object,
  source: Source,
  location: SourceLocation,
  sourceMaps: typeof SourceMaps
): Promise<SourceLocation> {
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

export async function getOriginalLocation(
  generatedLocation: SourceLocation,
  sourceMaps: typeof SourceMaps
) {
  if (isOriginalId(generatedLocation.sourceId)) {
    return location;
  }

  return sourceMaps.getOriginalLocation(generatedLocation);
}

export async function getMappedLocation(
  state: Object,
  sourceMaps: typeof SourceMaps,
  location: SourceLocation
): Promise<MappedLocation> {
  const source = getSource(state, location.sourceId);

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

export async function mapLocation(
  state: Object,
  sourceMaps: typeof SourceMaps,
  location: SourceLocation
): Promise<SourceLocation> {
  const source = getSource(state, location.sourceId);

  if (!source) {
    return location;
  }

  if (isOriginalId(location.sourceId)) {
    return getGeneratedLocation(state, source, location, sourceMaps);
  }

  return sourceMaps.getOriginalLocation(location);
}

export function isOriginalSource(source: ?Source) {
  return source && isOriginalId(source.id);
}
