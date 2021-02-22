/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Sources reducer
 * @module reducers/sources
 */

import { createSelector } from "reselect";
import {
  getPrettySourceURL,
  underRoot,
  getRelativeUrl,
  isGenerated,
  isOriginal as isOriginalSource,
  getPlainUrl,
  isPretty,
  isJavaScript,
} from "../utils/source";
import {
  createInitial,
  insertResources,
  updateResources,
  hasResource,
  getResource,
  getMappedResource,
  getResourceIds,
  memoizeResourceShallow,
  makeShallowQuery,
  makeReduceAllQuery,
  makeMapWithArgs,
} from "../utils/resource";
import { stripQuery } from "../utils/url";

import { findPosition } from "../utils/breakpoint/breakpointPositions";
import {
  pending,
  fulfilled,
  rejected,
  asSettled,
  isFulfilled,
} from "../utils/async-value";

import { originalToGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";

import {
  hasSourceActor,
  getSourceActor,
  getSourceActors,
  getAllThreadsBySource,
  getBreakableLinesForSourceActors,
} from "./source-actors";
import { getAllThreads } from "./threads";

import { uniq } from "lodash";

export function initialSourcesState(state) {
  return {
    sources: createInitial(),
    urls: {},
    plainUrls: {},
    sourcesWithUrls: [],
    content: {},
    actors: {},
    breakpointPositions: {},
    breakableLines: {},
    epoch: 1,
    selectedLocation: undefined,
    pendingSelectedLocation: prefs.pendingSelectedLocation,
    projectDirectoryRoot: prefs.projectDirectoryRoot,
    projectDirectoryRootName: prefs.projectDirectoryRootName,
    chromeAndExtensionsEnabled: prefs.chromeAndExtensionsEnabled,
    focusedItem: null,
    tabsBlackBoxed: state?.tabsBlackBoxed ?? [],
  };
}

function update(state = initialSourcesState(), action) {
  let location = null;

  switch (action.type) {
    case "ADD_SOURCE":
      return addSources(state, [action.source]);

    case "ADD_SOURCES":
      return addSources(state, action.sources);

    case "INSERT_SOURCE_ACTORS":
      return insertSourceActors(state, action);

    case "REMOVE_SOURCE_ACTORS":
      return removeSourceActors(state, action);

    case "SET_SELECTED_LOCATION":
      location = {
        ...action.location,
        url: action.source.url,
      };

      if (action.source.url) {
        prefs.pendingSelectedLocation = location;
      }

      return {
        ...state,
        selectedLocation: {
          sourceId: action.source.id,
          ...action.location,
        },
        pendingSelectedLocation: location,
      };

    case "CLEAR_SELECTED_LOCATION":
      location = { url: "" };
      prefs.pendingSelectedLocation = location;

      return {
        ...state,
        selectedLocation: null,
        pendingSelectedLocation: location,
      };

    case "SET_PENDING_SELECTED_LOCATION":
      location = {
        url: action.url,
        line: action.line,
        column: action.column,
      };

      prefs.pendingSelectedLocation = location;
      return { ...state, pendingSelectedLocation: location };

    case "LOAD_SOURCE_TEXT":
      return updateLoadedState(state, action);

    case "BLACKBOX_SOURCES":
      if (action.status === "done") {
        const { shouldBlackBox } = action;
        const { sources } = action.value;

        state = updateBlackBoxListSources(state, sources, shouldBlackBox);
        return updateBlackboxFlagSources(state, sources, shouldBlackBox);
      }
      break;

    case "BLACKBOX":
      if (action.status === "done") {
        const { id, url } = action.source;
        const { isBlackBoxed } = action.value;
        state = updateBlackBoxList(state, url, isBlackBoxed);
        return updateBlackboxFlag(state, id, isBlackBoxed);
      }
      break;

    case "SET_PROJECT_DIRECTORY_ROOT":
      const { url, name } = action;
      return updateProjectDirectoryRoot(state, url, name);

    case "SET_ORIGINAL_BREAKABLE_LINES": {
      const { breakableLines, sourceId } = action;
      return {
        ...state,
        breakableLines: {
          ...state.breakableLines,
          [sourceId]: breakableLines,
        },
      };
    }

    case "ADD_BREAKPOINT_POSITIONS": {
      const { source, positions } = action;
      const breakpointPositions = state.breakpointPositions[source.id];

      return {
        ...state,
        breakpointPositions: {
          ...state.breakpointPositions,
          [source.id]: { ...breakpointPositions, ...positions },
        },
      };
    }
    case "NAVIGATE":
      return {
        ...initialSourcesState(state),
        epoch: state.epoch + 1,
      };

    case "SET_FOCUSED_SOURCE_ITEM":
      return { ...state, focusedItem: action.item };
  }

  return state;
}

export const resourceAsSourceBase = memoizeResourceShallow(
  ({ content, ...source }) => source
);

const resourceAsSourceWithContent = memoizeResourceShallow(
  ({ content, ...source }) => ({
    ...source,
    content: asSettled(content),
  })
);

/*
 * Add sources to the sources store
 * - Add the source to the sources store
 * - Add the source URL to the urls map
 */
function addSources(state, sources) {
  const originalState = state;

  state = {
    ...state,
    urls: { ...state.urls },
    plainUrls: { ...state.plainUrls },
  };

  state.sources = insertResources(
    state.sources,
    sources.map(source => ({
      ...source,
      content: null,
    }))
  );

  for (const source of sources) {
    // 1. Update the source url map
    const existing = state.urls[source.url] || [];
    if (!existing.includes(source.id)) {
      state.urls[source.url] = [...existing, source.id];
    }

    // 2. Update the plain url map
    if (source.url) {
      const plainUrl = getPlainUrl(source.url);
      const existingPlainUrls = state.plainUrls[plainUrl] || [];
      if (!existingPlainUrls.includes(source.url)) {
        state.plainUrls[plainUrl] = [...existingPlainUrls, source.url];
      }

      // NOTE: we only want to copy the list once
      if (originalState.sourcesWithUrls === state.sourcesWithUrls) {
        state.sourcesWithUrls = [...state.sourcesWithUrls];
      }

      state.sourcesWithUrls.push(source.id);
    }
  }

  state = updateRootRelativeValues(state, sources);

  return state;
}

function insertSourceActors(state, action) {
  const { items } = action;
  state = {
    ...state,
    actors: { ...state.actors },
  };

  for (const sourceActor of items) {
    state.actors[sourceActor.source] = [
      ...(state.actors[sourceActor.source] || []),
      sourceActor.id,
    ];
  }

  const scriptActors = items.filter(
    item => item.introductionType === "scriptElement"
  );
  if (scriptActors.length > 0) {
    const { ...breakpointPositions } = state.breakpointPositions;

    // If new HTML sources are being added, we need to clear the breakpoint
    // positions since the new source is a <script> with new breakpoints.
    for (const { source } of scriptActors) {
      delete breakpointPositions[source];
    }

    state = { ...state, breakpointPositions };
  }

  return state;
}

/*
 * Update sources when the worker list changes.
 * - filter source actor lists so that missing threads no longer appear
 * - NOTE: we do not remove sources for destroyed threads
 */
function removeSourceActors(state, action) {
  const { items } = action;

  const actors = new Set(items.map(item => item.id));
  const sources = new Set(items.map(item => item.source));

  state = {
    ...state,
    actors: { ...state.actors },
  };

  for (const source of sources) {
    state.actors[source] = state.actors[source].filter(id => !actors.has(id));
  }

  return state;
}

/*
 * Update sources when the project directory root changes
 */
function updateProjectDirectoryRoot(state, root, name) {
  // Only update prefs when projectDirectoryRoot isn't a thread actor,
  // because when debugger is reopened, thread actor will change. See bug 1596323.
  if (actorType(root) !== "thread") {
    prefs.projectDirectoryRoot = root;
    prefs.projectDirectoryRootName = name;
  }

  return updateRootRelativeValues(state, undefined, root, name);
}

/* Checks if a path is a thread actor or not
 * e.g returns 'thread' for "server0.conn1.child1/workerTarget42/thread1"
 */
function actorType(actor) {
  const match = actor.match(/\/([a-z]+)\d+/);
  return match ? match[1] : null;
}

function updateRootRelativeValues(
  state,
  sources,
  projectDirectoryRoot = state.projectDirectoryRoot,
  projectDirectoryRootName = state.projectDirectoryRootName
) {
  const wrappedIdsOrIds = sources ? sources : getResourceIds(state.sources);

  state = {
    ...state,
    projectDirectoryRoot,
    projectDirectoryRootName,
  };

  const relativeURLUpdates = wrappedIdsOrIds.map(wrappedIdOrId => {
    const id =
      typeof wrappedIdOrId === "string" ? wrappedIdOrId : wrappedIdOrId.id;
    const source = getResource(state.sources, id);

    return {
      id,
      relativeUrl: getRelativeUrl(source, state.projectDirectoryRoot),
    };
  });

  state.sources = updateResources(state.sources, relativeURLUpdates);

  return state;
}

/*
 * Update a source's loaded text content.
 */
function updateLoadedState(state, action) {
  const { sourceId } = action;

  // If there was a navigation between the time the action was started and
  // completed, we don't want to update the store.
  if (action.epoch !== state.epoch || !hasResource(state.sources, sourceId)) {
    return state;
  }

  let content;
  if (action.status === "start") {
    content = pending();
  } else if (action.status === "error") {
    content = rejected(action.error);
  } else if (typeof action.value.text === "string") {
    content = fulfilled({
      type: "text",
      value: action.value.text,
      contentType: action.value.contentType,
    });
  } else {
    content = fulfilled({
      type: "wasm",
      value: action.value.text,
    });
  }

  return {
    ...state,
    sources: updateResources(state.sources, [
      {
        id: sourceId,
        content,
      },
    ]),
  };
}

/*
 * Update a source when its state changes
 * e.g. the text was loaded, it was blackboxed
 */
function updateBlackboxFlag(state, sourceId, isBlackBoxed) {
  // If there is no existing version of the source, it means that we probably
  // ended up here as a result of an async action, and the sources were cleared
  // between the action starting and the source being updated.
  if (!hasResource(state.sources, sourceId)) {
    // TODO: We may want to consider throwing here once we have a better
    // handle on async action flow control.
    return state;
  }

  return {
    ...state,
    sources: updateResources(state.sources, [
      {
        id: sourceId,
        isBlackBoxed,
      },
    ]),
  };
}

function updateBlackboxFlagSources(state, sources, shouldBlackBox) {
  const sourcesToUpdate = [];

  for (const source of sources) {
    if (!hasResource(state.sources, source.id)) {
      // TODO: We may want to consider throwing here once we have a better
      // handle on async action flow control.
      continue;
    }

    sourcesToUpdate.push({
      id: source.id,
      isBlackBoxed: shouldBlackBox,
    });
  }
  state.sources = updateResources(state.sources, sourcesToUpdate);

  return state;
}

function updateBlackboxTabs(tabs, url, isBlackBoxed) {
  const i = tabs.indexOf(url);
  if (i >= 0) {
    if (!isBlackBoxed) {
      tabs.splice(i, 1);
    }
  } else if (isBlackBoxed) {
    tabs.push(url);
  }
}

function updateBlackBoxList(state, url, isBlackBoxed) {
  const tabs = [...state.tabsBlackBoxed];
  updateBlackboxTabs(tabs, url, isBlackBoxed);
  return { ...state, tabsBlackBoxed: tabs };
}

function updateBlackBoxListSources(state, sources, shouldBlackBox) {
  const tabs = [...state.tabsBlackBoxed];

  sources.forEach(source => {
    updateBlackboxTabs(tabs, source.url, shouldBlackBox);
  });
  return { ...state, tabsBlackBoxed: tabs };
}

// Selectors

// Unfortunately, it's really hard to make these functions accept just
// the state that we care about and still type it with Flow. The
// problem is that we want to re-export all selectors from a single
// module for the UI, and all of those selectors should take the
// top-level app state, so we'd have to "wrap" them to automatically
// pick off the piece of state we're interested in. It's impossible
// (right now) to type those wrapped functions.

const getSourcesState = state => state.sources;

export function getSourceThreads(state, source) {
  return uniq(
    getSourceActors(state, state.sources.actors[source.id]).map(
      actor => actor.thread
    )
  );
}

export function getSourceInSources(sources, id) {
  return hasResource(sources, id)
    ? getMappedResource(sources, id, resourceAsSourceBase)
    : null;
}

export function getSource(state, id) {
  return getSourceInSources(getSources(state), id);
}

export function getSourceFromId(state, id) {
  const source = getSource(state, id);
  if (!source) {
    throw new Error(`source ${id} does not exist`);
  }
  return source;
}

export function getSourceByActorId(state, actorId) {
  if (!hasSourceActor(state, actorId)) {
    return null;
  }

  return getSource(state, getSourceActor(state, actorId).source);
}

export function getSourcesByURLInSources(sources, urls, url) {
  if (!url || !urls[url]) {
    return [];
  }
  return urls[url].map(id =>
    getMappedResource(sources, id, resourceAsSourceBase)
  );
}

export function getSourcesByURL(state, url) {
  return getSourcesByURLInSources(getSources(state), getUrls(state), url);
}

export function getSourceByURL(state, url) {
  const foundSources = getSourcesByURL(state, url);
  return foundSources ? foundSources[0] : null;
}

export function getSpecificSourceByURLInSources(
  sources,
  urls,
  url,
  isOriginal
) {
  const foundSources = getSourcesByURLInSources(sources, urls, url);
  if (foundSources) {
    return foundSources.find(source => isOriginalSource(source) == isOriginal);
  }
  return null;
}

export function getSpecificSourceByURL(state, url, isOriginal) {
  return getSpecificSourceByURLInSources(
    getSources(state),
    getUrls(state),
    url,
    isOriginal
  );
}

export function getOriginalSourceByURL(state, url) {
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

export function getGeneratedSourceById(state, sourceId) {
  const generatedSourceId = originalToGeneratedId(sourceId);
  return getSourceFromId(state, generatedSourceId);
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

const querySourceList = makeReduceAllQuery(resourceAsSourceBase, sources =>
  sources.slice()
);

export function getSources(state) {
  return state.sources.sources;
}

export function getSourcesEpoch(state) {
  return state.sources.epoch;
}

export function getUrls(state) {
  return state.sources.urls;
}

export function getPlainUrls(state) {
  return state.sources.plainUrls;
}

export function getSourceList(state) {
  return querySourceList(getSources(state));
}

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

export function getSourceCount(state) {
  return getSourceList(state).length;
}

export const getSelectedLocation = createSelector(
  getSourcesState,
  sources => sources.selectedLocation
);

export const getSelectedSource = createSelector(
  getSelectedLocation,
  getSources,
  (selectedLocation, sources) => {
    if (!selectedLocation) {
      return;
    }

    return getSourceInSources(sources, selectedLocation.sourceId);
  }
);

export const getSelectedSourceWithContent = createSelector(
  getSelectedLocation,
  getSources,
  (selectedLocation, sources) => {
    const source =
      selectedLocation &&
      getSourceInSources(sources, selectedLocation.sourceId);
    return source
      ? getMappedResource(sources, source.id, resourceAsSourceWithContent)
      : null;
  }
);
export function getSourceWithContent(state, id) {
  return getMappedResource(
    state.sources.sources,
    id,
    resourceAsSourceWithContent
  );
}
export function getSourceContent(state, id) {
  const { content } = getResource(state.sources.sources, id);
  return asSettled(content);
}

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

const queryAllDisplayedSources = makeShallowQuery({
  filter: (_, { sourcesWithUrls }) => sourcesWithUrls,
  map: makeMapWithArgs(
    (
      resource,
      ident,
      {
        projectDirectoryRoot,
        chromeAndExtensionsEnabled,
        debuggeeIsWebExtension,
        threads,
      }
    ) => ({
      id: resource.id,
      displayed:
        underRoot(resource, projectDirectoryRoot, threads) &&
        (!resource.isExtension ||
          chromeAndExtensionsEnabled ||
          debuggeeIsWebExtension),
    })
  ),
  reduce: items =>
    items.reduce((acc, { id, displayed }) => {
      if (displayed) {
        acc.push(id);
      }
      return acc;
    }, []),
});

function getAllDisplayedSources(state) {
  return queryAllDisplayedSources(state.sources.sources, {
    sourcesWithUrls: state.sources.sourcesWithUrls,
    projectDirectoryRoot: state.sources.projectDirectoryRoot,
    chromeAndExtensionsEnabled: state.sources.chromeAndExtensionsEnabled,
    debuggeeIsWebExtension: state.threads.isWebExtension,
    threads: getAllThreads(state),
  });
}

const getDisplayedSourceIDs = createSelector(
  getAllThreadsBySource,
  getAllDisplayedSources,
  (threadsBySource, displayedSources) => {
    const sourceIDsByThread = {};

    for (const sourceId of displayedSources) {
      const threads =
        threadsBySource[sourceId] ||
        threadsBySource[originalToGeneratedId(sourceId)] ||
        [];

      for (const thread of threads) {
        if (!sourceIDsByThread[thread]) {
          sourceIDsByThread[thread] = new Set();
        }
        sourceIDsByThread[thread].add(sourceId);
      }
    }
    return sourceIDsByThread;
  }
);

export const getDisplayedSources = createSelector(
  state => state.sources.sources,
  getDisplayedSourceIDs,
  (sources, idsByThread) => {
    const result = {};

    for (const thread of Object.keys(idsByThread)) {
      const entriesByNoQueryURL = Object.create(null);

      for (const id of idsByThread[thread]) {
        if (!result[thread]) {
          result[thread] = {};
        }
        const source = getResource(sources, id);

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
  const source = getSourceWithContent(state, id);
  if (
    !source ||
    isPretty(source) ||
    isOriginalSource(source) ||
    (prefs.clientSourceMapsEnabled && isSourceWithMap(state, id))
  ) {
    return false;
  }

  const sourceContent =
    source.content && isFulfilled(source.content) ? source.content.value : null;

  if (!sourceContent || !isJavaScript(source, sourceContent)) {
    return false;
  }

  return true;
}

export function getBreakpointPositions(state) {
  return state.sources.breakpointPositions;
}

export function getBreakpointPositionsForSource(state, sourceId) {
  const positions = getBreakpointPositions(state);
  return positions?.[sourceId];
}

export function hasBreakpointPositions(state, sourceId) {
  return !!getBreakpointPositionsForSource(state, sourceId);
}

export function getBreakpointPositionsForLine(state, sourceId, line) {
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return positions?.[line];
}

export function hasBreakpointPositionsForLine(state, sourceId, line) {
  return !!getBreakpointPositionsForLine(state, sourceId, line);
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

  if (isOriginalSource(source)) {
    return state.sources.breakableLines[sourceId];
  }

  // We pull generated file breakable lines directly from the source actors
  // so that breakable lines can be added as new source actors on HTML loads.
  return getBreakableLinesForSourceActors(
    state.sourceActors,
    state.sources.actors[sourceId]
  );
}

export const getSelectedBreakableLines = createSelector(
  state => {
    const sourceId = getSelectedSourceId(state);
    return sourceId && getBreakableLines(state, sourceId);
  },
  breakableLines => new Set(breakableLines || [])
);

export function isSourceLoadingOrLoaded(state, sourceId) {
  const { content } = getResource(state.sources.sources, sourceId);
  return content !== null;
}

export function getBlackBoxList(state) {
  return state.sources.tabsBlackBoxed;
}

export default update;
