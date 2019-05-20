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
  getPlainUrl,
} from "../utils/source";
import {
  createInitial,
  insertResources,
  updateResources,
  hasResource,
  getResource,
  getResourceIds,
  makeReduceQuery,
  makeReduceAllQuery,
  makeMapWithArgs,
  type Resource,
  type ResourceState,
  type ReduceQuery,
  type ReduceAllQuery,
} from "../utils/resource";

import { findPosition } from "../utils/breakpoint/breakpointPositions";
import * as asyncValue from "../utils/async-value";
import type { AsyncValue, SettledValue } from "../utils/async-value";
import { originalToGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";

import {
  hasSourceActor,
  getSourceActor,
  getSourceActors,
  getThreadsBySource,
  type SourceActorId,
  type SourceActorOuterState,
} from "./source-actors";
import type {
  Source,
  SourceId,
  SourceActor,
  SourceLocation,
  SourceContent,
  SourceWithContent,
  ThreadId,
  MappedLocation,
  BreakpointPositions,
} from "../types";
import type { PendingSelectedLocation, Selector } from "./types";
import type { Action, DonePromiseAction, FocusItem } from "../actions/types";
import type { LoadSourceAction } from "../actions/types/SourceAction";
import type { DebuggeeState } from "./debuggee";
import { uniq } from "lodash";

export type SourcesMap = { [SourceId]: Source };
type SourcesContentMap = {
  [SourceId]: AsyncValue<SourceContent> | null,
};
export type SourcesMapByThread = { [ThreadId]: SourcesMap };

export type BreakpointPositionsMap = { [SourceId]: BreakpointPositions };
type SourceActorMap = { [SourceId]: Array<SourceActorId> };

type UrlsMap = { [string]: SourceId[] };
type PlainUrlsMap = { [string]: string[] };

type SourceResource = Resource<{
  ...Source,
}>;
export type SourceResourceState = ResourceState<SourceResource>;

export type SourcesState = {
  epoch: number,

  // All known sources.
  sources: SourceResourceState,

  content: SourcesContentMap,

  breakpointPositions: BreakpointPositionsMap,
  breakableLines: { [SourceId]: Array<number> },

  // A link between each source object and the source actor they wrap over.
  actors: SourceActorMap,

  // All sources associated with a given URL. When using source maps, multiple
  // sources can have the same URL.
  urls: UrlsMap,

  // All full URLs belonging to a given plain (query string stripped) URL.
  // Query strings are only shown in the Sources tab if they are required for
  // disambiguation.
  plainUrls: PlainUrlsMap,

  pendingSelectedLocation?: PendingSelectedLocation,
  selectedLocation: ?SourceLocation,
  projectDirectoryRoot: string,
  chromeAndExtenstionsEnabled: boolean,
  focusedItem: ?FocusItem,
};

export function initialSourcesState(): SourcesState {
  return {
    sources: createInitial(),
    urls: {},
    plainUrls: {},
    content: {},
    actors: {},
    breakpointPositions: {},
    breakableLines: {},
    epoch: 1,
    selectedLocation: undefined,
    pendingSelectedLocation: prefs.pendingSelectedLocation,
    projectDirectoryRoot: prefs.projectDirectoryRoot,
    chromeAndExtenstionsEnabled: prefs.chromeAndExtenstionsEnabled,
    focusedItem: null,
  };
}

function update(
  state: SourcesState = initialSourcesState(),
  action: Action
): SourcesState {
  let location = null;

  switch (action.type) {
    case "CLEAR_SOURCE_MAP_URL":
      return clearSourceMaps(state, action.sourceId);
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
        return updateBlackboxFlag(state, id, isBlackBoxed);
      }
      break;

    case "SET_PROJECT_DIRECTORY_ROOT":
      return updateProjectDirectoryRoot(state, action.url);

    case "SET_BREAKABLE_LINES": {
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
        ...initialSourcesState(),
        epoch: state.epoch + 1,
      };

    case "SET_FOCUSED_SOURCE_ITEM":
      return { ...state, focusedItem: action.item };
  }

  return state;
}

function resourceAsSource(r: SourceResource): Source {
  return r;
}

/*
 * Add sources to the sources store
 * - Add the source to the sources store
 * - Add the source URL to the urls map
 */
function addSources(state: SourcesState, sources: Source[]): SourcesState {
  state = {
    ...state,
    content: { ...state.content },
    urls: { ...state.urls },
    plainUrls: { ...state.plainUrls },
  };

  state.sources = insertResources(state.sources, sources);

  for (const source of sources) {
    // 1. Add the source to the sources map
    state.content[source.id] = null;

    // 2. Update the source url map
    const existing = state.urls[source.url] || [];
    if (!existing.includes(source.id)) {
      state.urls[source.url] = [...existing, source.id];
    }

    // 3. Update the plain url map
    if (source.url) {
      const plainUrl = getPlainUrl(source.url);
      const existingPlainUrls = state.plainUrls[plainUrl] || [];
      if (!existingPlainUrls.includes(source.url)) {
        state.plainUrls[plainUrl] = [...existingPlainUrls, source.url];
      }
    }
  }

  state = updateRootRelativeValues(state, sources);

  return state;
}

function insertSourceActors(state: SourcesState, action): SourcesState {
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
function removeSourceActors(state: SourcesState, action) {
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
function updateProjectDirectoryRoot(state: SourcesState, root: string) {
  prefs.projectDirectoryRoot = root;

  return updateRootRelativeValues({
    ...state,
    projectDirectoryRoot: root,
  });
}

function updateRootRelativeValues(
  state: SourcesState,
  sources?: Array<Source>
) {
  const ids = sources
    ? sources.map(source => source.id)
    : getResourceIds(state.sources);

  state = {
    ...state,
  };

  const relativeURLUpdates = [];
  for (const id of ids) {
    const source = getResource(state.sources, id);

    relativeURLUpdates.push({
      id,
      relativeUrl: getRelativeUrl(source, state.projectDirectoryRoot),
    });
  }

  state.sources = updateResources(state.sources, relativeURLUpdates);

  return state;
}

/*
 * Update a source's loaded text content.
 */
function updateLoadedState(
  state: SourcesState,
  action: LoadSourceAction
): SourcesState {
  const { sourceId } = action;

  // If there was a navigation between the time the action was started and
  // completed, we don't want to update the store.
  if (action.epoch !== state.epoch || !(sourceId in state.content)) {
    return state;
  }

  let content;
  if (action.status === "start") {
    content = asyncValue.pending();
  } else if (action.status === "error") {
    content = asyncValue.rejected(action.error);
  } else if (typeof action.value.text === "string") {
    content = asyncValue.fulfilled({
      type: "text",
      value: action.value.text,
      contentType: action.value.contentType,
    });
  } else {
    content = asyncValue.fulfilled({
      type: "wasm",
      value: action.value.text,
    });
  }

  return {
    ...state,
    content: {
      ...state.content,
      [sourceId]: content,
    },
  };
}

function clearSourceMaps(
  state: SourcesState,
  sourceId: SourceId
): SourcesState {
  if (!hasResource(state.sources, sourceId)) {
    return state;
  }

  return {
    ...state,
    sources: updateResources(state.sources, [
      {
        id: sourceId,
        sourceMapURL: "",
      },
    ]),
  };
}

/*
 * Update a source when its state changes
 * e.g. the text was loaded, it was blackboxed
 */
function updateBlackboxFlag(
  state: SourcesState,
  sourceId: SourceId,
  isBlackBoxed: boolean
): SourcesState {
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
type DebuggeeOuterState = { debuggee: DebuggeeState };

const getSourcesState = (state: OuterState) => state.sources;

export function getSourceThreads(
  state: OuterState & SourceActorOuterState,
  source: Source
): ThreadId[] {
  return uniq(
    getSourceActors(state, state.sources.actors[source.id]).map(
      actor => actor.thread
    )
  );
}

export function getSourceInSources(
  sources: SourceResourceState,
  id: string
): ?Source {
  return hasResource(sources, id) ? getResource(sources, id) : null;
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
  state: OuterState & SourceActorOuterState,
  actorId: SourceActorId
): ?Source {
  if (!hasSourceActor(state, actorId)) {
    return null;
  }

  return getSource(state, getSourceActor(state, actorId).source);
}

export function getSourcesByURLInSources(
  sources: SourceResourceState,
  urls: UrlsMap,
  url: string
): Source[] {
  if (!url || !urls[url]) {
    return [];
  }
  return urls[url].map(id => getResource(sources, id));
}

export function getSourcesByURL(state: OuterState, url: string): Source[] {
  return getSourcesByURLInSources(getSources(state), getUrls(state), url);
}

export function getSourceByURL(state: OuterState, url: string): ?Source {
  const foundSources = getSourcesByURL(state, url);
  return foundSources ? foundSources[0] : null;
}

export function getSpecificSourceByURLInSources(
  sources: SourceResourceState,
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

export function getGeneratedSourceById(
  state: OuterState,
  sourceId: string
): Source {
  const generatedSourceId = originalToGeneratedId(sourceId);
  return getSourceFromId(state, generatedSourceId);
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
  url: ?string
): string[] {
  if (!url) {
    return [];
  }

  const plainUrl = getPlainUrl(url);
  return getPlainUrls(state)[plainUrl] || [];
}

export function getHasSiblingOfSameName(state: OuterState, source: ?Source) {
  if (!source) {
    return false;
  }

  return getSourcesUrlsInSources(state, source.url).length > 1;
}

const querySourceList: ReduceAllQuery<
  SourceResource,
  Array<Source>
> = makeReduceAllQuery(resourceAsSource, sources => sources.slice());

export function getSources(state: OuterState): SourceResourceState {
  return state.sources.sources;
}

export function getSourcesEpoch(state: OuterState) {
  return state.sources.epoch;
}

export function getUrls(state: OuterState) {
  return state.sources.urls;
}

export function getPlainUrls(state: OuterState) {
  return state.sources.plainUrls;
}

export function getSourceList(state: OuterState): Source[] {
  return querySourceList(getSources(state));
}

export function getDisplayedSourcesList(
  state: OuterState & SourceActorOuterState & DebuggeeOuterState
): Source[] {
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
  (
    selectedLocation: ?SourceLocation,
    sources: SourceResourceState
  ): ?Source => {
    if (!selectedLocation) {
      return;
    }

    return getSourceInSources(sources, selectedLocation.sourceId);
  }
);

type GSSWC = Selector<?SourceWithContent>;
export const getSelectedSourceWithContent: GSSWC = createSelector(
  getSelectedLocation,
  getSources,
  state => state.sources.content,
  (
    selectedLocation: ?SourceLocation,
    sources: SourceResourceState,
    content: SourcesContentMap
  ): SourceWithContent | null => {
    const source =
      selectedLocation &&
      getSourceInSources(sources, selectedLocation.sourceId);
    return source
      ? getSourceWithContentInner(sources, content, source.id)
      : null;
  }
);
export function getSourceWithContent(
  state: OuterState,
  id: SourceId
): SourceWithContent {
  return getSourceWithContentInner(
    state.sources.sources,
    state.sources.content,
    id
  );
}
export function getSourceContent(
  state: OuterState,
  id: SourceId
): SettledValue<SourceContent> | null {
  // Assert the resource exists.
  getResource(state.sources.sources, id);
  const content = state.sources.content[id];

  if (!content || content.state === "pending") {
    return null;
  }

  return content;
}

const contentLookup: WeakMap<Source, SourceWithContent> = new WeakMap();
function getSourceWithContentInner(
  sources: SourceResourceState,
  content: SourcesContentMap,
  id: SourceId
): SourceWithContent {
  const source = getResource(sources, id);
  let contentValue = content[source.id];

  let result = contentLookup.get(source);
  if (!result || result.content !== contentValue) {
    if (contentValue && contentValue.state === "pending") {
      contentValue = null;
    }
    result = {
      source,
      content: contentValue,
    };
    contentLookup.set(source, result);
  }

  return result;
}

export function getSelectedSourceId(state: OuterState) {
  const source = getSelectedSource((state: any));
  return source && source.id;
}

export function getProjectDirectoryRoot(state: OuterState): string {
  return state.sources.projectDirectoryRoot;
}

const queryAllDisplayedSources: ReduceQuery<
  SourceResource,
  {|
    projectDirectoryRoot: string,
    chromeAndExtensionsEnabled: boolean,
    debuggeeIsWebExtension: boolean,
  |},
  Array<SourceId>
> = makeReduceQuery(
  makeMapWithArgs(
    (
      resource,
      ident,
      {
        projectDirectoryRoot,
        chromeAndExtensionsEnabled,
        debuggeeIsWebExtension,
      }
    ) => ({
      id: resource.id,
      displayed:
        underRoot(resource, projectDirectoryRoot) &&
        (!resource.isExtension ||
          chromeAndExtensionsEnabled ||
          debuggeeIsWebExtension),
    })
  ),
  items =>
    items.reduce((acc, { id, displayed }) => {
      if (displayed) {
        acc.push(id);
      }
      return acc;
    }, [])
);

function getAllDisplayedSources(
  state: OuterState & DebuggeeOuterState
): Array<SourceId> {
  return queryAllDisplayedSources(state.sources.sources, {
    projectDirectoryRoot: state.sources.projectDirectoryRoot,
    chromeAndExtensionsEnabled: state.sources.chromeAndExtenstionsEnabled,
    debuggeeIsWebExtension: state.debuggee.isWebExtension,
  });
}

type GetDisplayedSourceIDsSelector = (
  OuterState & SourceActorOuterState & DebuggeeOuterState
) => { [ThreadId]: Set<SourceId> };
const getDisplayedSourceIDs: GetDisplayedSourceIDsSelector = createSelector(
  getThreadsBySource,
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

type GetDisplayedSourcesSelector = (
  OuterState & SourceActorOuterState & DebuggeeOuterState
) => SourcesMapByThread;
export const getDisplayedSources: GetDisplayedSourcesSelector = createSelector(
  state => state.sources.sources,
  getDisplayedSourceIDs,
  (sources, idsByThread) => {
    const result = {};

    for (const thread of Object.keys(idsByThread)) {
      for (const id of idsByThread[thread]) {
        if (!result[thread]) {
          result[thread] = {};
        }
        result[thread][id] = getResource(sources, id);
      }
    }

    return result;
  }
);

export function getSourceActorsForSource(
  state: OuterState & SourceActorOuterState,
  id: SourceId
): Array<SourceActor> {
  const actors = state.sources.actors[id];
  if (!actors) {
    return [];
  }

  return getSourceActors(state, actors);
}

export function canLoadSource(
  state: OuterState & SourceActorOuterState,
  sourceId: string
) {
  // Return false if we know that loadSourceText() will fail if called on this
  // source. This is used to avoid viewing such sources in the debugger.
  const source = getSource(state, sourceId);
  if (!source) {
    return false;
  }

  if (isOriginalSource(source)) {
    return true;
  }

  const actors = getSourceActorsForSource(state, sourceId);
  return actors.length != 0;
}

export function getBreakpointPositions(
  state: OuterState
): BreakpointPositionsMap {
  return state.sources.breakpointPositions;
}

export function getBreakpointPositionsForSource(
  state: OuterState,
  sourceId: string
): ?BreakpointPositions {
  const positions = getBreakpointPositions(state);
  return positions && positions[sourceId];
}

export function hasBreakpointPositions(
  state: OuterState,
  sourceId: string
): boolean {
  return !!getBreakpointPositionsForSource(state, sourceId);
}

export function hasBreakpointPositionsForLine(
  state: OuterState,
  sourceId: string,
  line: number
): boolean {
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return !!(positions && positions[line]);
}

export function getBreakpointPositionsForLocation(
  state: OuterState,
  location: SourceLocation
): ?MappedLocation {
  const { sourceId } = location;
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return findPosition(positions, location);
}

export function getBreakableLines(state: OuterState, sourceId: string) {
  if (!sourceId) {
    return null;
  }

  return state.sources.breakableLines[sourceId];
}

export const getSelectedBreakableLines: Selector<Set<number>> = createSelector(
  state => {
    const sourceId = getSelectedSourceId(state);
    return sourceId && state.sources.breakableLines[sourceId];
  },
  breakableLines => new Set(breakableLines || [])
);

export function isSourceLoadingOrLoaded(state: OuterState, sourceId: string) {
  const content = state.sources.content[sourceId];
  return content !== null;
}

export default update;
