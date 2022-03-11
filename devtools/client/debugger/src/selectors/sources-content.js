/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { asSettled } from "../utils/async-value";

import { getSelectedSource } from "../selectors/sources";

export function getSourceTextContent(state, id) {
  return state.sourcesContent.mutableTextContentMap.get(id);
}

export function getSourceContent(state, id) {
  const content = getSourceTextContent(state, id);
  return asSettled(content);
}

export function getSelectedSourceTextContent(state) {
  const source = getSelectedSource(state);
  if (!source) return null;
  return getSourceTextContent(state, source.id);
}

export function isSourceLoadingOrLoaded(state, sourceId) {
  const content = getSourceTextContent(state, sourceId);
  return content != null;
}

export function getSourcesEpoch(state) {
  return state.sourcesContent.epoch;
}
