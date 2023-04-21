/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { asSettled } from "../utils/async-value";

import {
  getSelectedLocation,
  getSource,
  getFirstSourceActorForGeneratedSource,
} from "../selectors/sources";

export function getSourceTextContent(state, location) {
  let { sourceId, sourceActorId } = location;

  const source = getSource(state, sourceId);
  if (!source) {
    return null;
  }

  if (source.isOriginal) {
    return state.sourcesContent.mutableOriginalSourceTextContentMapBySourceId.get(
      sourceId
    );
  }
  if (!sourceActorId) {
    const sourceActor = getFirstSourceActorForGeneratedSource(state, sourceId);
    sourceActorId = sourceActor.actor;
  }
  return state.sourcesContent.mutableGeneratedSourceTextContentMapBySourceActorId.get(
    sourceActorId
  );
}

export function getSettledSourceTextContent(state, location) {
  const content = getSourceTextContent(state, location);
  return asSettled(content);
}

export function getSelectedSourceTextContent(state) {
  const location = getSelectedLocation(state);

  if (!location) {
    return null;
  }

  return getSourceTextContent(state, location);
}

export function getSourcesEpoch(state) {
  return state.sourcesContent.epoch;
}
