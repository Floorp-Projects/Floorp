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
  makeReduceQuery,
  makeReduceAllQuery,
  makeMapWithArgs,
  type Resource,
  type ResourceState,
  type ReduceQuery,
  type ReduceAllQuery,
} from "../utils/resource";

import { findPosition } from "../utils/breakpoint/breakpointPositions";
import {
  pending,
  fulfilled,
  rejected,
  asSettled,
  isFulfilled,
} from "../utils/async-value";

import type { AsyncValue, SettledValue } from "../utils/async-value";
import { originalToGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";

import {
  hasSourceActor,
  getSourceActor,
  getSourceActors,
  getAllThreadsBySource,
  getBreakableLinesForSourceActors,
  type SourceActorId,
  type SourceActorOuterState,
} from "./source-actors";
import { getThreads, getMainThread } from "./threads";
import type {
  Source,
  SourceId,
  SourceActor,
  SourceLocation,
  SourceContent,
  SourceWithContent,
  ThreadId,
  MappedLocation,
  BreakpointPosition,
  BreakpointPositions,
  URL,
} from "../types";
import type { PendingSelectedLocation, Selector } from "./types";
import type { Action, DonePromiseAction, FocusItem } from "../actions/types";
import type { LoadSourceAction } from "../actions/types/SourceAction";
import type { ThreadsState } from "./threads";
import { uniq } from "lodash";

export type SourcesMap = { [SourceId]: Source };
export type SourcesMapByThread = { [ThreadId]: SourcesMap };

export type BreakpointPositionsMap = { [SourceId]: BreakpointPositions };
type SourceActorMap = { [SourceId]: Array<SourceActorId> };

type UrlsMap = { [string]: SourceId[] };
type PlainUrlsMap = { [string]: string[] };

export type SourceBase = {|
  +id: SourceId,
  +url: URL,
  +isBlackBoxed: boolean,
  +isPrettyPrinted: boolean,
  +relativeUrl: URL,
  +introductionUrl: ?URL,
  +introductionType: ?string,
  +extensionName: ?string,
  +isExtension: boolean,
  +isWasm: boolean,
  +isOriginal: boolean,
|};

export type SourceResource = Resource<{
  ...SourceBase,
  content: AsyncValue<SourceContent> | null,
}>;
export type SourceResourceState = ResourceState<SourceResource>;

export type SourcesState = {
  epoch: number,

  // All known sources.
  sources: SourceResourceState,

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
  chromeAndExtensionsEnabled: boolean,
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
    chromeAndExtensionsEnabled: prefs.chromeAndExtensionsEnabled,
    focusedItem: null,
  };
}

function update(
  state: SourcesState = initialSourcesState(),
  action: Action
): SourcesState {
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

        updateBlackBoxListSources(sources, shouldBlackBox);
        return updateBlackboxFlagSources(state, sources, shouldBlackBox);
      }
      break;

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
        ...initialSourcesState(),
        epoch: state.epoch + 1,
      };

    case "SET_FOCUSED_SOURCE_ITEM":
      return { ...state, focusedItem: action.item };
  }

  return state;
}

export const resourceAsSourceBase = memoizeResourceShallow(
  ({ content, ...source }: SourceResource): SourceBase => source
);

const resourceAsSourceWithContent = memoizeResourceShallow(
  ({ content, ...source }: SourceResource): SourceWithContent => ({
    ...source,
    content: asSettled(content),
  })
);

/*
 * Add sources to the sources store
 * - Add the source to the sources store
 * - Add the source URL to the urls map
 */
function addSources(state: SourcesState, sources: SourceBase[]): SourcesState {
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
  // Only update prefs when projectDirectoryRoot isn't a thread actor,
  // because when debugger is reopened, thread actor will change. See bug 1596323.
  if (actorType(root) !== "thread") {
    prefs.projectDirectoryRoot = root;
  }

  return updateRootRelativeValues(state, undefined, root);
}

/* Checks if a path is a thread actor or not
 * e.g returns 'thread' for "server0.conn1.child1/workerTarget42/thread1"
 */
function actorType(actor: string) {
  const match = actor.match(/\/([a-z]+)\d+/);
  return match ? match[1] : null;
}

function updateRootRelativeValues(
  state: SourcesState,
  sources?: $ReadOnlyArray<Source>,
  projectDirectoryRoot?: string = state.projectDirectoryRoot
) {
  const wrappedIdsOrIds: $ReadOnlyArray<Source> | Array<string> = sources
    ? sources
    : getResourceIds(state.sources);

  state = {
    ...state,
    projectDirectoryRoot,
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
function updateLoadedState(
  state: SourcesState,
  action: LoadSourceAction
): SourcesState {
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

function updateBlackBoxList(url, isBlackBoxed) {
  const tabs = getBlackBoxList();
  updateBlackboxTabs(tabs, url, isBlackBoxed);
  prefs.tabsBlackBoxed = tabs;
}

function updateBlackBoxListSources(sources, shouldBlackBox) {
  const tabs = getBlackBoxList();

  sources.forEach(source => {
    updateBlackboxTabs(tabs, source.url, shouldBlackBox);
  });

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
type ThreadsOuterState = { threads: ThreadsState };

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
  return hasResource(sources, id)
    ? getMappedResource(sources, id, resourceAsSourceBase)
    : null;
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
  url: URL
): Source[] {
  if (!url || !urls[url]) {
    return [];
  }
  return urls[url].map(id =>
    getMappedResource(sources, id, resourceAsSourceBase)
  );
}

export function getSourcesByURL(state: OuterState, url: URL): Source[] {
  return getSourcesByURLInSources(getSources(state), getUrls(state), url);
}

export function getSourceByURL(state: OuterState, url: URL): ?Source {
  const foundSources = getSourcesByURL(state, url);
  return foundSources ? foundSources[0] : null;
}

export function getSpecificSourceByURLInSources(
  sources: SourceResourceState,
  urls: UrlsMap,
  url: URL,
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
  url: URL,
  isOriginal: boolean
): ?Source {
  return getSpecificSourceByURLInSources(
    getSources(state),
    getUrls(state),
    url,
    isOriginal
  );
}

export function getOriginalSourceByURL(state: OuterState, url: URL): ?Source {
  return getSpecificSourceByURL(state, url, true);
}

export function getGeneratedSourceByURL(state: OuterState, url: URL): ?Source {
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
  sourceId: SourceId
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
  url: ?URL
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
> = makeReduceAllQuery(resourceAsSourceBase, sources => sources.slice());

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
  state: OuterState & SourceActorOuterState & ThreadsOuterState
): Source[] {
  return ((Object.values(getDisplayedSources(state)): any).flatMap(
    Object.values
  ): any);
}

export function getExtensionNameBySourceUrl(state: OuterState, url: URL) {
  const match = getSourceList(state).find(
    source => source.url && source.url.startsWith(url)
  );
  if (match && match.extensionName) {
    return match.extensionName;
  }
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
  (
    selectedLocation: ?SourceLocation,
    sources: SourceResourceState
  ): SourceWithContent | null => {
    const source =
      selectedLocation &&
      getSourceInSources(sources, selectedLocation.sourceId);
    return source
      ? getMappedResource(sources, source.id, resourceAsSourceWithContent)
      : null;
  }
);
export function getSourceWithContent(
  state: OuterState,
  id: SourceId
): SourceWithContent {
  return getMappedResource(
    state.sources.sources,
    id,
    resourceAsSourceWithContent
  );
}
export function getSourceContent(
  state: OuterState,
  id: SourceId
): SettledValue<SourceContent> | null {
  const { content } = getResource(state.sources.sources, id);
  return asSettled(content);
}

export function getSelectedSourceId(state: OuterState) {
  const source = getSelectedSource((state: any));
  return source?.id;
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
    threadActors: Array<ThreadId>,
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
        threadActors,
      }
    ) => ({
      id: resource.id,
      displayed:
        underRoot(resource, projectDirectoryRoot, threadActors) &&
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
  state: OuterState & ThreadsOuterState
): Array<SourceId> {
  return queryAllDisplayedSources(state.sources.sources, {
    projectDirectoryRoot: state.sources.projectDirectoryRoot,
    chromeAndExtensionsEnabled: state.sources.chromeAndExtensionsEnabled,
    debuggeeIsWebExtension: state.threads.isWebExtension,
    threadActors: [
      getMainThread(state).actor,
      ...getThreads(state).map(t => t.actor),
    ],
  });
}

type GetDisplayedSourceIDsSelector = (
  OuterState & SourceActorOuterState & ThreadsOuterState
) => { [ThreadId]: Set<SourceId> };
const getDisplayedSourceIDs: GetDisplayedSourceIDsSelector = createSelector(
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

type GetDisplayedSourcesSelector = (
  OuterState & SourceActorOuterState & ThreadsOuterState
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
  sourceId: SourceId
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

export function isSourceWithMap(
  state: OuterState & SourceActorOuterState,
  id: SourceId
): boolean {
  return getSourceActorsForSource(state, id).some(
    soureActor => soureActor.sourceMapURL
  );
}

export function canPrettyPrintSource(
  state: OuterState & SourceActorOuterState,
  id: SourceId
): boolean {
  const source: SourceWithContent = getSourceWithContent(state, id);
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

export function getBreakpointPositions(
  state: OuterState
): BreakpointPositionsMap {
  return state.sources.breakpointPositions;
}

export function getBreakpointPositionsForSource(
  state: OuterState,
  sourceId: SourceId
): ?BreakpointPositions {
  const positions = getBreakpointPositions(state);
  return positions?.[sourceId];
}

export function hasBreakpointPositions(
  state: OuterState,
  sourceId: SourceId
): boolean {
  return !!getBreakpointPositionsForSource(state, sourceId);
}

export function getBreakpointPositionsForLine(
  state: OuterState,
  sourceId: SourceId,
  line: number
): ?Array<BreakpointPosition> {
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return positions?.[line];
}

export function hasBreakpointPositionsForLine(
  state: OuterState,
  sourceId: SourceId,
  line: number
): boolean {
  return !!getBreakpointPositionsForLine(state, sourceId, line);
}

export function getBreakpointPositionsForLocation(
  state: OuterState,
  location: SourceLocation
): ?MappedLocation {
  const { sourceId } = location;
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return findPosition(positions, location);
}

export function getBreakableLines(
  state: OuterState & SourceActorOuterState,
  sourceId: SourceId
): ?Array<number> {
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

export const getSelectedBreakableLines: Selector<Set<number>> = createSelector(
  state => {
    const sourceId = getSelectedSourceId(state);
    return sourceId && getBreakableLines(state, sourceId);
  },
  breakableLines => new Set(breakableLines || [])
);

export function isSourceLoadingOrLoaded(state: OuterState, sourceId: SourceId) {
  const { content } = getResource(state.sources.sources, sourceId);
  return content !== null;
}

export default update;
