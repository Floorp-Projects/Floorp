/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SourceId, AstSource } from "./types";

const cachedSources: Map<SourceId, AstSource> = new Map();

export function setSource(source: AstSource) {
  cachedSources.set(source.id, source);
}

export function getSource(sourceId: SourceId): AstSource {
  const source = cachedSources.get(sourceId);
  if (!source) {
    throw new Error(`Parser: source ${sourceId} was not provided.`);
  }

  return source;
}

export function clearSources() {
  cachedSources.clear();
}
