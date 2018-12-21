/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Sources reducer
 * @module reducers/sources
 */

import { createSelector } from "../utils/createSelector";
import {
  getPrettySourceURL,
  underRoot,
  getRelativeUrl,
  isGenerated,
  isOriginal as isOriginalSource
} from "../utils/source";
import { originalToGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";

import type { Source, SourceId, SourceLocation } from "../types";
import type { PendingSelectedLocation } from "./types";
import type { Action, DonePromiseAction } from "../actions/types";
import type { LoadSourceAction } from "../actions/types/SourceAction";

export type SourcesMap = { [string]: Source };

type UrlsMap = { [string]: SourceId[] };

export type SourcesState = {
  sources: SourcesMap,
  urls: UrlsMap,
  relativeSources: SourcesMap,
  pendingSelectedLocation?: PendingSelectedLocation,
  selectedLocation: ?SourceLocation,
  projectDirectoryRoot: string
};

export function initialSourcesState(): SourcesState {
  return {
    sources: {},
    urls: {},
    relativeSources: {},
    selectedLocation: undefined,
    pendingSelectedLocation: prefs.pendingSelectedLocation,
    projectDirectoryRoot: prefs.projectDirectoryRoot
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
    case "UPDATE_SOURCE": {
      const source = action.source;
      return updateSources(state, [source]);
    }

    case "ADD_SOURCE": {
      const source = action.source;
      return updateSources(state, [source]);
    }

    case "ADD_SOURCES": {
      return updateSources(state, action.sources);
    }

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
      return recalculateRelativeSources(state, action.url);

    case "NAVIGATE":
      const source =
        state.selectedLocation &&
        state.sources[state.selectedLocation.sourceId];

      const url = source && source.url;

      if (!url) {
        return initialSourcesState();
      }

      return { ...initialSourcesState(), url };
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

function updateSources(state, sources) {
  state = {
    ...state,
    sources: { ...state.sources },
    relativeSources: { ...state.relativeSources },
    urls: { ...state.urls }
  };

  return sources.reduce(
    (newState, source) => updateSource(newState, source),
    state
  );
}

function updateSource(state: SourcesState, source: Object) {
  if (!source.id) {
    return state;
  }

  const existingSource = state.sources[source.id];
  const updatedSource = existingSource
    ? { ...existingSource, ...source }
    : createSource(source);

  state.sources[source.id] = updatedSource;

  const existingUrls = state.urls[source.url];
  state.urls[source.url] = existingUrls
    ? [...existingUrls, source.id]
    : [source.id];

  updateRelativeSource(
    state.relativeSources,
    updatedSource,
    state.projectDirectoryRoot
  );

  return state;
}

function updateRelativeSource(
  relativeSources: SourcesMap,
  source: Source,
  root: string
): SourcesMap {
  if (!underRoot(source, root)) {
    return relativeSources;
  }

  const relativeSource: Source = ({
    ...source,
    relativeUrl: getRelativeUrl(source, root)
  }: any);

  relativeSources[source.id] = relativeSource;

  return relativeSources;
}

function recalculateRelativeSources(state: SourcesState, root: string) {
  prefs.projectDirectoryRoot = root;

  const relativeSources = (Object.values(state.sources): any).reduce(
    (sources, source: Source) => updateRelativeSource(sources, source, root),
    {}
  );

  return {
    ...state,
    projectDirectoryRoot: root,
    relativeSources
  };
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

export function getSource(state: OuterState, id: string) {
  return getSourceInSources(getSources(state), id);
}

export function getSourceFromId(state: OuterState, id: string): Source {
  return getSourcesState(state).sources[id];
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

export function getGeneratedSource(state: OuterState, source: Source): Source {
  if (isGenerated(source)) {
    return source;
  }

  return getSourceFromId(state, originalToGeneratedId(source.id));
}

export function getPendingSelectedLocation(state: OuterState) {
  return state.sources.pendingSelectedLocation;
}

export function getPrettySource(state: OuterState, id: string) {
  const source = getSource(state, id);
  if (!source) {
    return;
  }

  return getSpecificSourceByURL(state, getPrettySourceURL(source.url), true);
}

export function hasPrettySource(state: OuterState, id: string) {
  return !!getPrettySource(state, id);
}

export function getOriginalSourceByUrlInSources(
  sources: SourcesMap,
  urls: UrlsMap,
  url: string
) {
  const foundSources = getSourcesByUrlInSources(sources, urls, url);
  if (!foundSources) {
    return null;
  }

  return foundSources.find(source => isOriginalSource(source) == true);
}
export function getGeneratedSourceByUrlInSources(
  sources: SourcesMap,
  urls: UrlsMap,
  url: string
) {
  const foundSources = getSourcesByUrlInSources(sources, urls, url);
  if (!foundSources) {
    return null;
  }

  return foundSources.find(source => isOriginalSource(source) == false);
}

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

export const getSourceCount = createSelector(
  getSources,
  sources => Object.keys(sources).length
);

export const getSelectedLocation = createSelector(
  getSourcesState,
  sources => sources.selectedLocation
);

export const getSelectedSource = createSelector(
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

export function getRelativeSources(state: OuterState): SourcesMap {
  return state.sources.relativeSources;
}

export default update;
