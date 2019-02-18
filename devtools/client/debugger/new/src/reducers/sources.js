/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

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
  isUrlExtension
} from "../utils/source";

import { originalToGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";

import type {
  Source,
  SourceActor,
  SourceId,
  SourceLocation,
  ThreadId,
  WorkerList
} from "../types";
import type { PendingSelectedLocation, Selector } from "./types";
import type { Action, DonePromiseAction, FocusItem } from "../actions/types";
import type { LoadSourceAction } from "../actions/types/SourceAction";
import { omitBy, mapValues } from "lodash";

export type SourcesMap = { [SourceId]: Source };
export type SourcesMapByThread = { [ThreadId]: SourcesMap };

type SourceActorsMap = { [SourceId]: SourceActor[] };
type UrlsMap = { [string]: SourceId[] };
type GetRelativeSourcesSelector = OuterState => SourcesMapByThread;

export type SourcesState = {
  // All known sources.
  sources: SourcesMap,

  // Actors associated with each source.
  sourceActors: SourceActorsMap,

  // All sources associated with a given URL. When using source maps, multiple
  // sources can have the same URL.
  urls: UrlsMap,

  // All original sources associated with a generated source.
  originalSources: { [SourceId]: SourceId[] },

  // For each thread, all sources in that thread that are under the project root
  // and should be shown in the editor's sources pane.
  relativeSources: SourcesMapByThread,

  pendingSelectedLocation?: PendingSelectedLocation,
  selectedLocation: ?SourceLocation,
  projectDirectoryRoot: string,
  chromeAndExtenstionsEnabled: boolean,
  focusedItem: ?FocusItem
};

export function initialSourcesState(): SourcesState {
  return {
    sources: {},
    sourceActors: {},
    urls: {},
    originalSources: {},
    relativeSources: {},
    selectedLocation: undefined,
    pendingSelectedLocation: prefs.pendingSelectedLocation,
    projectDirectoryRoot: prefs.projectDirectoryRoot,
    chromeAndExtenstionsEnabled: prefs.chromeAndExtenstionsEnabled,
    focusedItem: null
  };
}

export function createSource(source: Object): Source {
  return {
    id: undefined,
    url: undefined,
    sourceMapURL: undefined,
    isBlackBoxed: false,
    isPrettyPrinted: false,
    isWasm: false,
    isExtension: (source.url && isUrlExtension(source.url)) || false,
    text: undefined,
    contentType: "",
    error: undefined,
    loadedState: "unloaded",
    ...source
  };
}

function update(
  state: SourcesState = initialSourcesState(),
  action: Action
): SourcesState {
  let location = null;

  switch (action.type) {
    case "UPDATE_SOURCE":
      return updateSources(state, [action.source]);

    case "ADD_SOURCE":
      return updateSources(state, [action.source]);

    case "ADD_SOURCES":
      return updateSources(state, action.sources, action.sourceActors);

    case "SET_WORKERS":
      return updateWorkers(state, action.workers, action.mainThread);

    case "SET_SELECTED_LOCATION":
      location = {
        ...action.location,
        url: action.source.url
      };

      if (action.source.url) {
        prefs.pendingSelectedLocation = location;
      }

      return {
        ...state,
        selectedLocation: {
          sourceId: action.source.id,
          ...action.location
        },
        pendingSelectedLocation: location
      };

    case "CLEAR_SELECTED_LOCATION":
      location = { url: "" };
      prefs.pendingSelectedLocation = location;

      return {
        ...state,
        selectedLocation: null,
        pendingSelectedLocation: location
      };

    case "SET_PENDING_SELECTED_LOCATION":
      location = {
        url: action.url,
        line: action.line
      };

      prefs.pendingSelectedLocation = location;
      return { ...state, pendingSelectedLocation: location };

    case "LOAD_SOURCE_TEXT":
      return setSourceTextProps(state, action);

    case "BLACKBOX":
      if (action.status === "done") {
        const { id, url } = action.source;
        const { isBlackBoxed } = ((action: any): DonePromiseAction).value;
        updateBlackBoxList(url, isBlackBoxed);
        return updateSources(state, [{ id, isBlackBoxed }]);
      }
      break;

    case "SET_PROJECT_DIRECTORY_ROOT":
      return updateProjectDirectoryRoot(state, action.url);

    case "NAVIGATE":
      return initialSourcesState();

    case "SET_FOCUSED_SOURCE_ITEM":
      return { ...state, focusedItem: action.item };
  }

  return state;
}

function getTextPropsFromAction(action) {
  const { sourceId } = action;

  if (action.status === "start") {
    return { id: sourceId, loadedState: "loading" };
  } else if (action.status === "error") {
    return { id: sourceId, error: action.error, loadedState: "loaded" };
  }

  if (!action.value) {
    return null;
  }

  return {
    id: sourceId,
    text: action.value.text,
    contentType: action.value.contentType,
    loadedState: "loaded"
  };
}

// TODO: Action is coerced to `any` unfortunately because how we type
// asynchronous actions is wrong. The `value` may be null for the
// "start" and "error" states but we don't type it like that. We need
// to rethink how we type async actions.
function setSourceTextProps(state, action: LoadSourceAction): SourcesState {
  const source = getTextPropsFromAction(action);
  if (!source) {
    return state;
  }
  return updateSources(state, [source]);
}

function updateSources(state, sources, sourceActors) {
  const relativeSources = { ...state.relativeSources };
  for (const thread in relativeSources) {
    relativeSources[thread] = { ...relativeSources[thread] };
  }

  state = {
    ...state,
    sources: { ...state.sources },
    sourceActors: { ...state.sourceActors },
    urls: { ...state.urls },
    originalSources: { ...state.originalSources },
    relativeSources
  };

  sources.forEach(source => updateSource(state, source));
  if (sourceActors) {
    sourceActors.forEach(sourceActor =>
      updateForNewSourceActor(state, sourceActor)
    );
  }

  return state;
}

function updateSourceUrl(state: SourcesState, source: Object) {
  const existing = state.urls[source.url] || [];
  if (!existing.includes(source.id)) {
    state.urls[source.url] = [...existing, source.id];
  }
}

function updateOriginalSources(state: SourcesState, source: Object) {
  if (!isOriginalSource(source)) {
    return;
  }
  const generatedId = originalToGeneratedId(source.id);
  const existing = state.originalSources[generatedId] || [];
  if (!existing.includes(source.id)) {
    state.originalSources[generatedId] = [...existing, source.id];

    // Update relative sources for any affected threads.
    if (state.sourceActors[generatedId]) {
      for (const sourceActor of state.sourceActors[generatedId]) {
        updateRelativeSource(state, source, sourceActor);
      }
    }
  }
}

function updateSource(state: SourcesState, source: Object) {
  if (!source.id) {
    return;
  }

  const existingSource = state.sources[source.id];
  const updatedSource = existingSource
    ? { ...existingSource, ...source }
    : createSource(source);

  state.sources[source.id] = updatedSource;

  updateExistingRelativeSource(state, source);
  updateSourceUrl(state, source);
  updateOriginalSources(state, source);
}

function updateExistingRelativeSource(state: SourcesState, source: Object) {
  const relativeSources = { ...state.relativeSources };

  for (const thread in relativeSources) {
    if (relativeSources[thread][source.id]) {
      relativeSources[thread] = { ...relativeSources[thread] };
      const existingRelativeSource = relativeSources[thread][source.id];
      const updatedRelativeSource = { ...existingRelativeSource, ...source };
      state.relativeSources[thread][source.id] = updatedRelativeSource;
    }
  }
}

function updateRelativeSource(
  state: SourcesState,
  source: Object,
  sourceActor: SourceActor
) {
  const root = state.projectDirectoryRoot;

  if (!underRoot(source, root)) {
    return;
  }

  const relativeSource: Source = ({
    ...source,
    relativeUrl: getRelativeUrl(source, root)
  }: any);

  if (!state.relativeSources[sourceActor.thread]) {
    state.relativeSources[sourceActor.thread] = {};
  }
  state.relativeSources[sourceActor.thread][source.id] = relativeSource;
}

function updateForNewSourceActor(
  state: SourcesState,
  sourceActor: SourceActor
) {
  const existing = state.sourceActors[sourceActor.source] || [];

  // Do not allow duplicate source actors in the store.
  if (existing.some(({ actor }) => actor == sourceActor.actor)) {
    return;
  }

  state.sourceActors[sourceActor.source] = [...existing, sourceActor];

  updateRelativeSource(state, state.sources[sourceActor.source], sourceActor);
}

function updateWorkers(
  state: SourcesState,
  workers: WorkerList,
  mainThread: ThreadId
) {
  // Clear out actors for any removed workers by regenerating source actor
  // state for all remaining workers.
  const sourceActors = [];
  for (const actors: any of Object.values(state.sourceActors)) {
    for (const sourceActor of actors) {
      if (
        workers.some(worker => worker.actor == sourceActor.thread) ||
        mainThread == sourceActor.thread
      ) {
        sourceActors.push(sourceActor);
      }
    }
  }

  return updateSources(
    { ...state, sourceActors: {}, relativeSources: {} },
    [],
    sourceActors
  );
}

function updateProjectDirectoryRoot(state: SourcesState, root: string) {
  prefs.projectDirectoryRoot = root;

  const sourceActors = [];
  for (const actors: any of Object.values(state.sourceActors)) {
    actors.forEach(sourceActor => sourceActors.push(sourceActor));
  }

  return updateSources(
    {
      ...state,
      projectDirectoryRoot: root,
      sourceActors: {},
      relativeSources: {}
    },
    [],
    sourceActors
  );
}

function updateBlackBoxList(url, isBlackBoxed) {
  const tabs = getBlackBoxList();
  const i = tabs.indexOf(url);
  if (i >= 0) {
    if (!isBlackBoxed) {
      tabs.splice(i, 1);
    }
  } else if (isBlackBoxed) {
    tabs.push(url);
  }
  prefs.tabsBlackBoxed = tabs;
}

export function getBlackBoxList() {
  return prefs.tabsBlackBoxed || [];
}

// Selectors

// Unfortunately, it's really hard to make these functions accept just
// the state that we care about and still type it with Flow. The
// problem is that we want to re-export all selectors from a single
// module for the UI, and all of those selectors should take the
// top-level app state, so we'd have to "wrap" them to automatically
// pick off the piece of state we're interested in. It's impossible
// (right now) to type those wrapped functions.
type OuterState = { sources: SourcesState };

const getSourcesState = (state: OuterState) => state.sources;

export function getSource(state: OuterState, id: SourceId) {
  return getSourceInSources(getSources(state), id);
}

export function getSourceFromId(state: OuterState, id: string): Source {
  const source = getSourcesState(state).sources[id];
  if (!source) {
    throw new Error(`source ${id} does not exist`);
  }
  return source;
}

export function getSourceActors(state: OuterState, id: SourceId) {
  return getSourcesState(state).sourceActors[id] || [];
}

export function getSourceActor(state: OuterState, id: SourceId) {
  return getSourceActors(state, id)[0];
}

export function hasSourceActor(state: OuterState, sourceActor: SourceActor) {
  const existing = getSourceActors(state, sourceActor.source);
  return existing.some(({ actor }) => actor == sourceActor.actor);
}

export function getOriginalSourceByURL(
  state: OuterState,
  url: string
): ?Source {
  return getOriginalSourceByUrlInSources(
    getSources(state),
    getUrls(state),
    url
  );
}

export function getGeneratedSourceByURL(
  state: OuterState,
  url: string
): ?Source {
  return getGeneratedSourceByUrlInSources(
    getSources(state),
    getUrls(state),
    url
  );
}

export function getSpecificSourceByURL(
  state: OuterState,
  url: string,
  isOriginal: boolean
): ?Source {
  return isOriginal
    ? getOriginalSourceByUrlInSources(getSources(state), getUrls(state), url)
    : getGeneratedSourceByUrlInSources(getSources(state), getUrls(state), url);
}

export function getSourceByURL(state: OuterState, url: string): ?Source {
  return getSourceByUrlInSources(getSources(state), getUrls(state), url);
}

export function getSourcesByURLs(state: OuterState, urls: string[]) {
  return urls.map(url => getSourceByURL(state, url)).filter(Boolean);
}

export function getSourcesByURL(state: OuterState, url: string): Source[] {
  return getSourcesByUrlInSources(getSources(state), getUrls(state), url);
}

export function getGeneratedSource(
  state: OuterState,
  source: ?Source
): ?Source {
  if (!source) {
    return null;
  }

  if (isGenerated(source)) {
    return source;
  }

  return getSourceFromId(state, originalToGeneratedId(source.id));
}

export function getPendingSelectedLocation(state: OuterState) {
  return state.sources.pendingSelectedLocation;
}

export function getPrettySource(state: OuterState, id: ?string) {
  if (!id) {
    return;
  }

  const source = getSource(state, id);
  if (!source) {
    return;
  }

  return getSpecificSourceByURL(state, getPrettySourceURL(source.url), true);
}

export function hasPrettySource(state: OuterState, id: string) {
  return !!getPrettySource(state, id);
}

function getSourceHelper(
  original: boolean,
  sources: SourcesMap,
  urls: UrlsMap,
  url: string
) {
  const foundSources = getSourcesByUrlInSources(sources, urls, url);
  if (!foundSources) {
    return null;
  }

  return foundSources.find(source => isOriginalSource(source) == original);
}

export const getOriginalSourceByUrlInSources = getSourceHelper.bind(null, true);

export const getGeneratedSourceByUrlInSources = getSourceHelper.bind(
  null,
  false
);

export function getSpecificSourceByUrlInSources(
  sources: SourcesMap,
  urls: UrlsMap,
  url: string,
  isOriginal: boolean
) {
  return isOriginal
    ? getOriginalSourceByUrlInSources(sources, urls, url)
    : getGeneratedSourceByUrlInSources(sources, urls, url);
}

export function getSourceByUrlInSources(
  sources: SourcesMap,
  urls: UrlsMap,
  url: string
) {
  const foundSources = getSourcesByUrlInSources(sources, urls, url);
  if (!foundSources) {
    return null;
  }

  return foundSources[0];
}

function getSourcesByUrlInSources(
  sources: SourcesMap,
  urls: UrlsMap,
  url: string
) {
  if (!url || !urls[url]) {
    return [];
  }

  return urls[url].map(id => sources[id]);
}

export function getSourcesUrlsInSources(
  state: OuterState,
  url: string
): string[] {
  const urls = getUrls(state);
  if (!url || !urls[url]) {
    return [];
  }
  const plainUrl = url.split("?")[0];

  return Object.keys(urls)
    .filter(Boolean)
    .filter(sourceUrl => sourceUrl.split("?")[0] === plainUrl);
}

export function getHasSiblingOfSameName(state: OuterState, source: ?Source) {
  if (!source) {
    return false;
  }

  return getSourcesUrlsInSources(state, source.url).length > 1;
}

export function getSourceInSources(sources: SourcesMap, id: string): ?Source {
  return sources[id];
}

export function getSources(state: OuterState) {
  return state.sources.sources;
}

export function getUrls(state: OuterState) {
  return state.sources.urls;
}

export function getSourceList(state: OuterState): Source[] {
  return (Object.values(getSources(state)): any);
}

export function getRelativeSourcesList(state: OuterState): Source[] {
  return ((Object.values(getRelativeSources(state)): any).flatMap(
    Object.values
  ): any);
}

export function getSourceCount(state: OuterState) {
  return getSourceList(state).length;
}

export const getSelectedLocation: Selector<?SourceLocation> = createSelector(
  getSourcesState,
  sources => sources.selectedLocation
);

export const getSelectedSource: Selector<?Source> = createSelector(
  getSelectedLocation,
  getSources,
  (selectedLocation: ?SourceLocation, sources: SourcesMap): ?Source => {
    if (!selectedLocation) {
      return;
    }

    return sources[selectedLocation.sourceId];
  }
);

export function getProjectDirectoryRoot(state: OuterState): string {
  return state.sources.projectDirectoryRoot;
}

function getAllRelativeSources(state: OuterState): SourcesMapByThread {
  return state.sources.relativeSources;
}

function getChromeAndExtenstionsEnabled(state: OuterState) {
  return state.sources.chromeAndExtenstionsEnabled;
}

export const getRelativeSources: GetRelativeSourcesSelector = createSelector(
  getChromeAndExtenstionsEnabled,
  getAllRelativeSources,
  (chromeAndExtenstionsEnabled, relativeSources) => {
    if (!chromeAndExtenstionsEnabled) {
      return mapValues(relativeSources, threadSources => {
        return omitBy(threadSources, source => source.isExtension);
      });
    }
    return relativeSources;
  }
);

export function getRelativeSourcesForThread(
  state: OuterState,
  thread: string
): SourcesMap {
  return getRelativeSources(state)[thread] || {};
}

export function getFocusedSourceItem(state: OuterState): ?FocusItem {
  return state.sources.focusedItem;
}

export default update;
