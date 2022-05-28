/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { shallowEqual } from "../utils/shallow-equal";

import {
  getPrettySourceURL,
  isDescendantOfRoot,
  isGenerated,
  getPlainUrl,
  isPretty,
  isJavaScript,
  removeThreadActorId,
} from "../utils/source";
import { stripQuery } from "../utils/url";

import { findPosition } from "../utils/breakpoint/breakpointPositions";
import { isFulfilled } from "../utils/async-value";

import { originalToGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";

import {
  hasSourceActor,
  getSourceActor,
  getSourceActors,
  getBreakableLinesForSourceActors,
} from "./source-actors";
import { getSourceTextContent } from "./sources-content";
import { getAllThreads } from "./threads";

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
    return;
  }

  const source = getSource(state, id);
  if (!source) {
    return;
  }

  return getOriginalSourceByURL(state, getPrettySourceURL(source.url));
}

export function hasPrettySource(state, id) {
  return !!getPrettySource(state, id);
}

// This is only used by jest tests
export function getSourcesUrlsInSources(state, url) {
  if (!url) {
    return [];
  }

  const plainUrl = getPlainUrl(url);
  return getPlainUrls(state)[plainUrl] || [];
}

export function getHasSiblingOfSameName(state, source) {
  if (!source) {
    return false;
  }

  return getSourcesUrlsInSources(state, source.url).length > 1;
}

// This is only used externaly by tabs and breakpointSources selectors
export function getSourcesMap(state) {
  return state.sources.sources;
}

function getUrls(state) {
  return state.sources.urls;
}

function getPlainUrls(state) {
  return state.sources.plainUrls;
}

export const getSourceList = createSelector(
  getSourcesMap,
  sourcesMap => {
    return [...sourcesMap.values()];
  },
  { equalityCheck: shallowEqual, resultEqualityCheck: shallowEqual }
);

export function getDisplayedSourcesList(state) {
  return Object.values(getDisplayedSources(state)).flatMap(Object.values);
}

export function getExtensionNameBySourceUrl(state, url) {
  const match = getSourceList(state).find(
    source => source.url && source.url.startsWith(url)
  );
  if (match && match.extensionName) {
    return match.extensionName;
  }
}

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
      return;
    }

    return sourcesMap.get(selectedLocation.sourceId);
  }
);

// This is used by tests and pause reducers
export function getSelectedSourceId(state) {
  const source = getSelectedSource(state);
  return source?.id;
}

export function getProjectDirectoryRoot(state) {
  return state.sources.projectDirectoryRoot;
}

export function getProjectDirectoryRootName(state) {
  return state.sources.projectDirectoryRootName;
}

const getDisplayedSourceIDs = createSelector(
  getSourcesMap,
  state => state.sources.sourcesWithUrls,
  state => state.sources.projectDirectoryRoot,
  state => state.sources.chromeAndExtensionsEnabled,
  state => state.threads.isWebExtension,
  getAllThreads,
  (
    sourcesMap,
    sourcesWithUrls,
    projectDirectoryRoot,
    chromeAndExtensionsEnabled,
    debuggeeIsWebExtension,
    threads
  ) => {
    const rootWithoutThreadActor = removeThreadActorId(
      projectDirectoryRoot,
      threads
    );
    const sourceIDsByThread = {};

    for (const id of sourcesWithUrls) {
      const source = sourcesMap.get(id);

      const displayed =
        isDescendantOfRoot(source, rootWithoutThreadActor) &&
        (!source.isExtension ||
          chromeAndExtensionsEnabled ||
          debuggeeIsWebExtension);
      if (!displayed) {
        continue;
      }

      const thread = source.thread;
      if (!sourceIDsByThread[thread]) {
        sourceIDsByThread[thread] = new Set();
      }
      sourceIDsByThread[thread].add(id);
    }
    return sourceIDsByThread;
  }
);

export const getDisplayedSources = createSelector(
  getSourcesMap,
  getDisplayedSourceIDs,
  (sourcesMap, idsByThread) => {
    const result = {};

    for (const thread of Object.keys(idsByThread)) {
      const entriesByNoQueryURL = Object.create(null);

      for (const id of idsByThread[thread]) {
        if (!result[thread]) {
          result[thread] = {};
        }
        const source = sourcesMap.get(id);

        const entry = {
          ...source,
          displayURL: source.url,
        };
        result[thread][id] = entry;

        const noQueryURL = stripQuery(entry.displayURL);
        if (!entriesByNoQueryURL[noQueryURL]) {
          entriesByNoQueryURL[noQueryURL] = [];
        }
        entriesByNoQueryURL[noQueryURL].push(entry);
      }

      // If the URL does not compete with another without the query string,
      // we exclude the query string when rendering the source URL to keep the
      // UI more easily readable.
      for (const noQueryURL in entriesByNoQueryURL) {
        const entries = entriesByNoQueryURL[noQueryURL];
        if (entries.length === 1) {
          entries[0].displayURL = noQueryURL;
        }
      }
    }

    return result;
  }
);

export function getSourceActorsForSource(state, id) {
  const actors = state.sources.actors[id];
  if (!actors) {
    return [];
  }

  return getSourceActors(state, actors);
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

  const sourceActorIDs = state.sources.actors[sourceId];
  if (!sourceActorIDs?.length) {
    return null;
  }

  // We pull generated file breakable lines directly from the source actors
  // so that breakable lines can be added as new source actors on HTML loads.
  return getBreakableLinesForSourceActors(state, sourceActorIDs, source.isHTML);
}

export const getSelectedBreakableLines = createSelector(
  state => {
    const sourceId = getSelectedSourceId(state);
    return sourceId && getBreakableLines(state, sourceId);
  },
  breakableLines => new Set(breakableLines || [])
);

export function getBlackBoxRanges(state) {
  return state.sources.blackboxedRanges;
}
