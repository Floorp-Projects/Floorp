/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isOriginalId } from "devtools-source-map";
import { getSource } from "../selectors";
import { isGenerated } from "../utils/source";

import type { SourceLocation, MappedLocation, Source } from "../types";
import typeof SourceMaps from "../../packages/devtools-source-map/src";

export async function getGeneratedLocation(
  state: Object,
  source: Source,
  location: SourceLocation,
  sourceMaps: Object
): Promise<SourceLocation> {
  if (!isOriginalId(location.sourceId)) {
    return location;
  }

  const { line, sourceId, column } = await sourceMaps.getGeneratedLocation(
    location,
    source
  );

  const generatedSource = getSource(state, sourceId);
  if (!generatedSource) {
    throw new Error(`Could not find generated source ${sourceId}`);
  }

  return {
    line,
    sourceId,
    column: column === 0 ? undefined : column,
    sourceUrl: generatedSource.url
  };
}

export async function getOriginalLocation(
  generatedLocation: SourceLocation,
  source: Source,
  sourceMaps: SourceMaps
) {
  if (isOriginalId(generatedLocation.sourceId)) {
    return location;
  }

  return sourceMaps.getOriginalLocation(generatedLocation);
}

export async function getMappedLocation(
  state: Object,
  sourceMaps: Object,
  location: SourceLocation
): Promise<SourceLocation> {
  const source = getSource(state, location.sourceId);

  if (!source) {
    return location;
  }

  if (isOriginalId(location.sourceId)) {
    return getGeneratedLocation(state, source, location, sourceMaps);
  }

  return sourceMaps.getOriginalLocation(location, source);
}

export function getSelectedLocation(
  mappedLocation: MappedLocation,
  selectedSource: ?Source
) {
  return selectedSource && isGenerated(selectedSource)
    ? mappedLocation.generatedLocation
    : mappedLocation.location;
}
