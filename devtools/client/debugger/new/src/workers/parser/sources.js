/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Source, SourceId } from "../../types";

let cachedSources = new Map();

export function hasSource(sourceId: SourceId): boolean {
  return cachedSources.has(sourceId);
}

export function setSource(source: Source) {
  cachedSources.set(source.id, source);
}

export function getSource(sourceId: SourceId): Source {
  if (!cachedSources.has(sourceId)) {
    throw new Error(`Parser: source ${sourceId} was not provided.`);
  }

  return ((cachedSources.get(sourceId): any): Source);
}

export function clearSources() {
  cachedSources = new Map();
}
