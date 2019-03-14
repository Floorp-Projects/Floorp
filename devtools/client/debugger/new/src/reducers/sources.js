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

import type { Source, SourceId, SourceLocation, ThreadId } from "../types";
import type { PendingSelectedLocation, Selector } from "./types";
import type { Action, DonePromiseAction, FocusItem } from "../actions/types";
import type { LoadSourceAction } from "../actions/types/SourceAction";
import { mapValues, uniqBy, uniq } from "lodash";

export type SourcesMap = { [SourceId]: Source };
export type SourcesMapByThread = { [ThreadId]: SourcesMap };

type UrlsMap = { [string]: SourceId[] };
type DisplayedSources = { [ThreadId]: { [SourceId]: boolean } };
type GetDisplayedSourcesSelector = OuterState => { [ThreadId]: SourcesMap };

export type SourcesState = {
  // All known sources.
  sources: SourcesMap,

  // All sources associated with a given URL. When using source maps, multiple
  // sources can have the same URL.
  urls: UrlsMap,

  // For each thread, all sources in that thread that are under the project root
  // and should be shown in the editor's sources pane.
  displayed: DisplayedSources,

  pendingSelectedLocation?: PendingSelectedLocation,
  selectedLocation: ?SourceLocation,
  projectDirectoryRoot: string,
  chromeAndExtenstionsEnabled: boolean,
  focusedItem: ?FocusItem
};

const emptySources = {
  sources: {},
  urls: {},
  displayed: {}
};

export function initialSourcesState(): SourcesState {
  return {
    ...emptySources,
    selectedLocation: undefined,
    pendingSelectedLocation: prefs.pendingSelectedLocation,
    projectDirectoryRoot: prefs.projectDirectoryRoot,
    chromeAndExtenstionsEnabled: prefs.chromeAndExtenstionsEnabled,
    focusedItem: null
  };
}

export function createSource(state: SourcesState, source: Object): Source {
  const root = state.projectDirectoryRoot;
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
    relativeUrl: getRelativeUrl(source, root),
    actors: [],
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
      return updateSource(state, action.source);

    case "ADD_SOURCE":
      return addSources(state, [action.source]);

    case "ADD_SOURCES":
      return addSources(state, action.sources);

    case "SET_WORKERS":
      return updateWorkers(state, action);

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
      return updateLoadedState(state, action);

    case "BLACKBOX":
      if (action.status === "done") {
        const { id, url } = action.source;
        const { isBlackBoxed } = ((action: any): DonePromiseAction).value;
        updateBlackBoxList(url, isBlackBoxed);
        return updateSource(state, { id, isBlackBoxed });
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

/*
 * Update a source when its state changes
 * e.g. the text was loaded, it was blackboxed
 */
function updateSource(state: SourcesState, source: Object) {
  const existingSource = state.sources[source.id];

  // If there is no existing version of the source, it means that we probably
  // ended up here as a result of an async action, and the sources were cleared
  // between the action starting and the source being updated.
  if (!existingSource) {
    // TODO: We may want to consider throwing here once we have a better
    // handle on async action flow control.
    return state;
  }
  return {
    ...state,
    sources: {
      ...state.sources,
      [source.id]: { ...existingSource, ...source }
    }
  };
}

/*
 * Update all of the sources when an event occurs.
 * e.g. workers are updated, project directory root changes
 */
function updateAllSources(state: SourcesState, callback: any) {
  const updatedSources = Object.values(state.sources).map(source => ({
    ...source,
    ...callback(source)
  }));

  return addSources({ ...state, ...emptySources }, updatedSources);
}

/*
 * Add sources to the sources store
 * - Add the source to the sources store
 * - Add the source URL to the urls map
 * - Add the source ID to the thread displayed map
 */
function addSources(state: SourcesState, sources: Source[]) {
  state = {
    ...state,
    sources: { ...state.sources },
    urls: { ...state.urls },
    displayed: { ...state.displayed }
  };

  for (const source of sources) {
    const existingSource = state.sources[source.id];
    let updatedSource = existingSource || source;

    // Merge the source actor list
    if (existingSource && source.actors) {
      const actors = uniqBy(
        [...existingSource.actors, ...source.actors],
        ({ actor }) => actor
      );

      updatedSource = (({ ...updatedSource, actors }: any): Source);
    }

    // 1. Add the source to the sources map
    state.sources[source.id] = updatedSource;

    // 2. Update the source url map
    const existing = state.urls[source.url] || [];
    if (!existing.includes(source.id)) {
      state.urls[source.url] = [...existing, source.id];
    }

    // 3. Update the displayed actor map
    if (
      underRoot(source, state.projectDirectoryRoot) &&
      (!source.isExtension ||
        getChromeAndExtenstionsEnabled({ sources: state }))
    ) {
      for (const actor of getSourceActors(state, source)) {
        if (!state.displayed[actor.thread]) {
          state.displayed[actor.thread] = {};
        }
        state.displayed[actor.thread][source.id] = true;
      }
    }
  }

  return state;
}

/*
 * Update sources when the worker list changes.
 * - filter source actor lists so that missing threads no longer appear
 * - NOTE: we do not remove sources for destroyed threads
 */
function updateWorkers(state: SourcesState, action: Object) {
  const threads = [
    action.mainThread,
    ...action.workers.map(({ actor }) => actor)
  ];

  return updateAllSources(state, source => ({
    actors: source.actors.filter(({ thread }) => threads.includes(thread))
  }));
}

/*
 * Update sources when the project directory root changes
 */
function updateProjectDirectoryRoot(state: SourcesState, root: string) {
  prefs.projectDirectoryRoot = root;

  return updateAllSources({ ...state, projectDirectoryRoot: root }, source => ({
    relativeUrl: getRelativeUrl(source, root)
  }));
}

/*
 * Update a source's loaded state fields
 * i.e. loadedState, text, error
 */
function updateLoadedState(state, action: LoadSourceAction): SourcesState {
  const { sourceId } = action;
  let source;

  if (action.status === "start") {
    source = { id: sourceId, loadedState: "loading" };
  } else if (action.status === "error") {
    source = { id: sourceId, error: action.error, loadedState: "loaded" };
  } else {
    if (!action.value) {
      return state;
    }

    source = {
      id: sourceId,
      text: action.value.text,
      contentType: action.value.contentType,
      loadedState: "loaded"
    };
  }

  return updateSource(state, source);
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

function getSourceActors(state, source) {
  if (isGenerated(source)) {
    return source.actors;
  }

  // Original sources do not have actors, so use the generated source.
  const generatedSource = state.sources[originalToGeneratedId(source.id)];
  return generatedSource ? generatedSource.actors : [];
}

export function getSourceThreads(
  state: OuterState,
  source: Source
): ThreadId[] {
  return uniq(
    getSourceActors(state.sources, source).map(actor => actor.thread)
  );
}

export function getSourceInSources(sources: SourcesMap, id: string): ?Source {
  return sources[id];
}

export function getSource(state: OuterState, id: SourceId): ?Source {
  return getSourceInSources(getSources(state), id);
}

export function getSourceFromId(state: OuterState, id: string): Source {
  const source = getSource(state, id);
  if (!source) {
    throw new Error(`source ${id} does not exist`);
  }
  return source;
}

export function getSourceByActorId(
  state: OuterState,
  actorId: string
): ?Source {
  // We don't index the sources by actor IDs, so this method should be used
  // sparingly.
  for (const source of getSourceList(state)) {
    if (source.actors.some(({ actor }) => actor == actorId)) {
      return source;
    }
  }
  return null;
}

export function getSourcesByURLInSources(
  sources: SourcesMap,
  urls: UrlsMap,
  url: string
): Source[] {
  if (!url || !urls[url]) {
    return [];
  }
  return urls[url].map(id => sources[id]);
}

export function getSourcesByURL(state: OuterState, url: string): Source[] {
  return getSourcesByURLInSources(getSources(state), getUrls(state), url);
}

export function getSourceByURL(state: OuterState, url: string): ?Source {
  const foundSources = getSourcesByURL(state, url);
  return foundSources ? foundSources[0] : null;
}

export function getSpecificSourceByURLInSources(
  sources: SourcesMap,
  urls: UrlsMap,
  url: string,
  isOriginal: boolean
): ?Source {
  const foundSources = getSourcesByURLInSources(sources, urls, url);
  if (foundSources) {
    return foundSources.find(source => isOriginalSource(source) == isOriginal);
  }
  return null;
}

export function getSpecificSourceByURL(
  state: OuterState,
  url: string,
  isOriginal: boolean
): ?Source {
  return getSpecificSourceByURLInSources(
    getSources(state),
    getUrls(state),
    url,
    isOriginal
  );
}

export function getOriginalSourceByURL(
  state: OuterState,
  url: string
): ?Source {
  return getSpecificSourceByURL(state, url, true);
}

export function getGeneratedSourceByURL(
  state: OuterState,
  url: string
): ?Source {
  return getSpecificSourceByURL(state, url, false);
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

  return getOriginalSourceByURL(state, getPrettySourceURL(source.url));
}

export function hasPrettySource(state: OuterState, id: string) {
  return !!getPrettySource(state, id);
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

export function getSources(state: OuterState) {
  return state.sources.sources;
}

export function getUrls(state: OuterState) {
  return state.sources.urls;
}

export function getSourceList(state: OuterState): Source[] {
  return (Object.values(getSources(state)): any);
}

export function getDisplayedSourcesList(state: OuterState): Source[] {
  return ((Object.values(getDisplayedSources(state)): any).flatMap(
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

export function getSelectedSourceId(state: OuterState) {
  const source = getSelectedSource((state: any));
  return source && source.id;
}

export function getProjectDirectoryRoot(state: OuterState): string {
  return state.sources.projectDirectoryRoot;
}

function getAllDisplayedSources(state: OuterState) {
  return state.sources.displayed;
}

function getChromeAndExtenstionsEnabled(state: OuterState) {
  return state.sources.chromeAndExtenstionsEnabled;
}

export const getDisplayedSources: GetDisplayedSourcesSelector = createSelector(
  getSources,
  getChromeAndExtenstionsEnabled,
  getAllDisplayedSources,
  (sources, chromeAndExtenstionsEnabled, displayed) => {
    return mapValues(displayed, threadSourceIds =>
      mapValues(threadSourceIds, (_, id) => sources[id])
    );
  }
);

export function getDisplayedSourcesForThread(
  state: OuterState,
  thread: string
): SourcesMap {
  return getDisplayedSources(state)[thread] || {};
}

export function getFocusedSourceItem(state: OuterState): ?FocusItem {
  return state.sources.focusedItem;
}

export default update;
