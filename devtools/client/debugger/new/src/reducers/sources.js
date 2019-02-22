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
  SourceId,
  SourceLocation,
  ThreadId,
  WorkerList
} from "../types";
import type { PendingSelectedLocation, Selector } from "./types";
import type { Action, DonePromiseAction, FocusItem } from "../actions/types";
import type { LoadSourceAction } from "../actions/types/SourceAction";
import { mapValues, uniqBy } from "lodash";

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
      return updateSources(state, [action.source]);

    case "ADD_SOURCE":
      return updateSources(state, [action.source]);

    case "ADD_SOURCES":
      return updateSources(state, action.sources);

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

function updateSources(state, sources: Object[]) {
  const displayed = { ...state.displayed };
  for (const thread in displayed) {
    displayed[thread] = { ...displayed[thread] };
  }

  state = {
    ...state,
    sources: { ...state.sources },
    urls: { ...state.urls },
    displayed
  };

  sources.forEach(source => updateSource(state, source));
  return state;
}

function updateSourceUrl(state: SourcesState, source: Source) {
  const existing = state.urls[source.url] || [];
  if (!existing.includes(source.id)) {
    state.urls[source.url] = [...existing, source.id];
  }
}

function updateSource(state: SourcesState, source: Object) {
  if (!source.id) {
    return;
  }

  const existingSource = state.sources[source.id];
  const updatedSource = existingSource
    ? { ...existingSource, ...source }
    : createSource(state, source);

  // Any actors in the source are added to the existing ones.
  if (existingSource && source.actors) {
    updatedSource.actors = uniqBy(
      [...existingSource.actors, ...updatedSource.actors],
      ({ actor }) => actor
    );
  }

  state.sources[source.id] = updatedSource;

  updateSourceUrl(state, updatedSource);
  updateDisplayedSource(state, updatedSource);
}

function updateDisplayedSource(state: SourcesState, source: Source) {
  const root = state.projectDirectoryRoot;

  if (
    !underRoot(source, root) ||
    (!getChromeAndExtenstionsEnabled({ sources: state }) && source.isExtension)
  ) {
    return;
  }

  let actors = source.actors;

  // Original sources do not have actors, so use the generated source.
  if (isOriginalSource(source)) {
    const generatedSource = state.sources[originalToGeneratedId(source.id)];
    actors = generatedSource ? generatedSource.actors : [];
  }

  actors.forEach(({ thread }) => {
    if (!state.displayed[thread]) {
      state.displayed[thread] = {};
    }
    state.displayed[thread][source.id] = true;
  });
}

function updateWorkers(
  state: SourcesState,
  workers: WorkerList,
  mainThread: ThreadId
) {
  // Filter out source actors for all removed threads.
  const threads = [mainThread, ...workers.map(({ actor }) => actor)];
  const updateActors = source =>
    source.actors.filter(({ thread }) => threads.includes(thread));

  const sources: Source[] = Object.values(state.sources)
    .map((source: any) => ({ ...source, actors: updateActors(source) }))

  // Regenerate derived information from the updated sources.
  return updateSources({ ...state, ...emptySources }, sources);
}

function updateProjectDirectoryRoot(state: SourcesState, root: string) {
  prefs.projectDirectoryRoot = root;

  const sources: Source[] = Object.values(state.sources).map((source: any) => {
    return {
      ...source,
      relativeUrl: getRelativeUrl(source, root)
    };
  });

  // Regenerate derived information from the updated sources.
  return updateSources(
    { ...state, projectDirectoryRoot: root, ...emptySources },
    sources
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
