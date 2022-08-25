/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { shallowEqual } from "../utils/shallow-equal";

import {
  getPrettySourceURL,
  isGenerated,
  isPretty,
  isJavaScript,
} from "../utils/source";

import { findPosition } from "../utils/breakpoint/breakpointPositions";
import { isFulfilled } from "../utils/async-value";

import { originalToGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";

import {
  hasSourceActor,
  getSourceActor,
  getBreakableLinesForSourceActors,
} from "./source-actors";
import { getSourceTextContent } from "./sources-content";

export function hasSource(state, id) {
  return state.sources.sources.has(id);
}

export function getSource(state, id) {
  return state.sources.sources.get(id);
}

export function getSourceFromId(state, id) {
  const source = getSource(state, id);
  if (!source) {
    console.warn(`source ${id} does not exist`);
  }
  return source;
}

export function getLocationSource(state, location) {
  return getSource(state, location.sourceId);
}

export function getSourceByActorId(state, actorId) {
  if (!hasSourceActor(state, actorId)) {
    return null;
  }

  return getSource(state, getSourceActor(state, actorId).source);
}

function getSourcesByURL(state, url) {
  const urls = getUrls(state);
  if (!url || !urls[url]) {
    return [];
  }
  return urls[url].map(id => getSource(state, id));
}

export function getSourceByURL(state, url) {
  const foundSources = getSourcesByURL(state, url);
  return foundSources ? foundSources[0] : null;
}

// This is used by tabs selectors
export function getSpecificSourceByURL(state, url, isOriginal) {
  const foundSources = getSourcesByURL(state, url);
  if (foundSources) {
    return foundSources.find(source => source.isOriginal == isOriginal);
  }
  return null;
}

function getOriginalSourceByURL(state, url) {
  return getSpecificSourceByURL(state, url, true);
}

export function getGeneratedSourceByURL(state, url) {
  return getSpecificSourceByURL(state, url, false);
}

export function getGeneratedSource(state, source) {
  if (!source) {
    return null;
  }

  if (isGenerated(source)) {
    return source;
  }

  return getSourceFromId(state, originalToGeneratedId(source.id));
}

export function getPendingSelectedLocation(state) {
  return state.sources.pendingSelectedLocation;
}

export function getPrettySource(state, id) {
  if (!id) {
    return null;
  }

  const source = getSource(state, id);
  if (!source) {
    return null;
  }

  return getOriginalSourceByURL(state, getPrettySourceURL(source.url));
}

export function hasPrettySource(state, id) {
  return !!getPrettySource(state, id);
}

// This is only used externaly by tabs and breakpointSources selectors
export function getSourcesMap(state) {
  return state.sources.sources;
}

function getUrls(state) {
  return state.sources.urls;
}

export const getSourceList = createSelector(
  getSourcesMap,
  sourcesMap => {
    return [...sourcesMap.values()];
  },
  { equalityCheck: shallowEqual, resultEqualityCheck: shallowEqual }
);

// This is only used by tests
export function getSourceCount(state) {
  return getSourcesMap(state).size;
}

export function getSelectedLocation(state) {
  return state.sources.selectedLocation;
}

export const getSelectedSource = createSelector(
  getSelectedLocation,
  getSourcesMap,
  (selectedLocation, sourcesMap) => {
    if (!selectedLocation) {
      return undefined;
    }

    return sourcesMap.get(selectedLocation.sourceId);
  }
);

// This is used by tests and pause reducers
export function getSelectedSourceId(state) {
  const source = getSelectedSource(state);
  return source?.id;
}
/**
 * Get the source actor of the source
 *
 * @param {Object} state
 * @param {String} id
 *        The source id
 * @return {Array<Object>}
 *         List of source actors
 */
export function getSourceActorsForSource(state, id) {
  const actorsInfo = state.sources.actors[id];
  if (!actorsInfo) {
    return [];
  }

  return actorsInfo
    .map(actorInfo => getSourceActor(state, actorInfo.id))
    .filter(actor => !!actor);
}

export function isSourceWithMap(state, id) {
  return getSourceActorsForSource(state, id).some(
    sourceActor => sourceActor.sourceMapURL
  );
}

export function canPrettyPrintSource(state, id) {
  const source = getSource(state, id);
  if (
    !source ||
    isPretty(source) ||
    source.isOriginal ||
    (prefs.clientSourceMapsEnabled && isSourceWithMap(state, id))
  ) {
    return false;
  }

  const content = getSourceTextContent(state, id);
  const sourceContent = content && isFulfilled(content) ? content.value : null;

  if (!sourceContent || !isJavaScript(source, sourceContent)) {
    return false;
  }

  return true;
}

// Used by visibleColumnBreakpoints selectors
export function getBreakpointPositions(state) {
  return state.sources.breakpointPositions;
}

export function getBreakpointPositionsForSource(state, sourceId) {
  const positions = getBreakpointPositions(state);
  return positions?.[sourceId];
}

// This is only used by one test
export function hasBreakpointPositions(state, sourceId) {
  return !!getBreakpointPositionsForSource(state, sourceId);
}

export function getBreakpointPositionsForLine(state, sourceId, line) {
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return positions?.[line];
}

export function getBreakpointPositionsForLocation(state, location) {
  const { sourceId } = location;
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return findPosition(positions, location);
}

export function getBreakableLines(state, sourceId) {
  if (!sourceId) {
    return null;
  }
  const source = getSource(state, sourceId);
  if (!source) {
    return null;
  }

  if (source.isOriginal) {
    return state.sources.breakableLines[sourceId];
  }

  const sourceActorsInfo = state.sources.actors[sourceId];
  if (!sourceActorsInfo?.length) {
    return null;
  }

  // We pull generated file breakable lines directly from the source actors
  // so that breakable lines can be added as new source actors on HTML loads.
  return getBreakableLinesForSourceActors(
    state,
    sourceActorsInfo.map(actorInfo => actorInfo.id),
    source.isHTML
  );
}

export const getSelectedBreakableLines = createSelector(
  state => {
    const sourceId = getSelectedSourceId(state);
    return sourceId && getBreakableLines(state, sourceId);
  },
  breakableLines => new Set(breakableLines || [])
);
