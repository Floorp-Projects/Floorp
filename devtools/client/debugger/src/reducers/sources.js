/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Sources reducer
 * @module reducers/sources
 */

import { getRelativeUrl, getPlainUrl } from "../utils/source";
import {
  createInitial,
  insertResources,
  updateResources,
  hasResource,
  getResource,
  getResourceIds,
} from "../utils/resource";
import { pending, fulfilled, rejected } from "../utils/async-value";
import { prefs } from "../utils/prefs";

export function initialSourcesState(state) {
  return {
    /**
     * All currently available sources.
     *
     * See create.js: `createSourceObject` method for the description of stored objects.
     * This reducers will add an extra `content` attribute which is the source text for each source.
     */
    sources: createInitial(),

    /**
     * All sources associated with a given URL. When using source maps, multiple
     * sources can have the same URL.
     *
     * Dictionary(url => array<source id>)
     */
    urls: {},

    /**
     * All full URLs belonging to a given plain (query string stripped) URL.
     * Query strings are only shown in the Sources tab if they are required for
     * disambiguation.
     *
     * Dictionary(plain url => array<source url>)
     */
    plainUrls: {},

    /**
     * List of all source ids whose source has a url attribute defined
     *
     * Array<source id>
     */
    sourcesWithUrls: [],

    /**
     * Mapping of source id's to one or more source-actor id's.
     * Dictionary whose keys are source id's and values are arrays
     * made of all the related source-actor id's.
     *
     * "source" are the objects stored in this reducer, in the `sources` attribute.
     * "source-actor" are the objects stored in the "source-actors.js" reducer, in its `sourceActors` attribute.
     *
     * Dictionary(source id => array<SourceActor ID>)
     */
    actors: {},

    breakpointPositions: {},
    breakableLines: {},

    /**
     * Incremental number that is bumped each time we navigate to a new page.
     *
     * This is used to better handle async race condition where we mix previous page data
     * with the new page. As sources are keyed by URL we may easily conflate the two page loads data.
     */
    epoch: 1,

    /**
     * The actual currently selected location.
     * Only set if the related source is already registered in the sources reducer.
     * Otherwise, pendingSelectedLocation should be used. Typically for sources
     * which are about to be created.
     *
     * It also includes line and column information.
     *
     * See `createLocation` for the definition of this object.
     */
    selectedLocation: undefined,

    /**
     * When we want to select a source that isn't available yet, use this.
     * The location object should have a url attribute instead of a sourceId.
     */
    pendingSelectedLocation: prefs.pendingSelectedLocation,

    /**
     * Project root set from the Source Tree.
     *
     * This focused the source tree on a subset of sources.
     * `relativeUrl` attribute of all sources will be updated according
     * to the new root.
     */
    projectDirectoryRoot: prefs.projectDirectoryRoot,
    projectDirectoryRootName: prefs.projectDirectoryRootName,

    /**
     * Boolean, to be set to true in order to display WebExtension's content scripts
     * that are applied to the current page we are debugging.
     *
     * Covered by: browser_dbg-content-script-sources.js
     * Bound to: devtools.chrome.enabled
     *
     * boolean
     */
    chromeAndExtensionsEnabled: prefs.chromeAndExtensionsEnabled,

    /* FORMAT:
     * blackboxedRanges: {
     *  [source url]: [range, range, ...], -- source lines blackboxed
     *  [source url]: [], -- whole source blackboxed
     *  ...
     * }
     */
    blackboxedRanges: state?.blackboxedRanges ?? {},
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

    case "BLACKBOX":
      if (action.status === "done") {
        const { blackboxSources } = action.value;
        state = updateBlackBoxState(state, blackboxSources);
        // This is always called after `updateBlackBoxState` as the updated
        // state is used to update the `isBlackBoxed` property on the source.
        return updateSourcesBlackboxState(state, blackboxSources);
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
  }

  return state;
}

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

  // The `sourceActor` objects are defined from `newGeneratedSources` action:
  // https://searchfox.org/mozilla-central/rev/4646b826a25d3825cf209db890862b45fa09ffc3/devtools/client/debugger/src/actions/sources/newSources.js#300-314
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
 * Update the "isBlackBoxed" property on the source objects
 */
function updateSourcesBlackboxState(state, blackboxSources) {
  const sourcesToUpdate = [];

  for (const { source } of blackboxSources) {
    if (!hasResource(state.sources, source.id)) {
      // TODO: We may want to consider throwing here once we have a better
      // handle on async action flow control.
      continue;
    }

    // The `isBlackBoxed` flag on the source should be `true` when the source still
    // has blackboxed lines or the whole source is blackboxed.
    const isBlackBoxed = !!state.blackboxedRanges[source.url];
    sourcesToUpdate.push({
      id: source.id,
      isBlackBoxed,
    });
  }
  state.sources = updateResources(state.sources, sourcesToUpdate);

  return state;
}

function updateBlackboxRangesForSourceUrl(
  currentRanges,
  url,
  shouldBlackBox,
  newRanges
) {
  if (shouldBlackBox) {
    // If newRanges is an empty array, it would mean we are blackboxing the whole
    // source. To do that lets reset the contentto an empty array.
    if (!newRanges.length) {
      currentRanges[url] = [];
    } else {
      currentRanges[url] = currentRanges[url] || [];
      newRanges.forEach(newRange => {
        // To avoid adding duplicate ranges make sure
        // no range alredy exists with same start and end lines.
        const duplicate = currentRanges[url].findIndex(
          r =>
            r.start.line == newRange.start.line &&
            r.end.line == newRange.end.line
        );
        if (duplicate !== -1) {
          return;
        }
        // ranges are sorted in asc
        const index = currentRanges[url].findIndex(
          range =>
            range.end.line <= newRange.start.line &&
            range.end.column <= newRange.start.column
        );
        currentRanges[url].splice(index + 1, 0, newRange);
      });
    }
  } else {
    // if there are no ranges to blackbox, then we are unblackboxing
    // the whole source
    if (!newRanges.length) {
      delete currentRanges[url];
      return;
    }
    // Remove only the lines represented by the ranges provided.
    newRanges.forEach(newRange => {
      const index = currentRanges[url].findIndex(
        range =>
          range.start.line === newRange.start.line &&
          range.end.line === newRange.end.line
      );

      if (index !== -1) {
        currentRanges[url].splice(index, 1);
      }
    });

    // if the last blackboxed line has been removed, unblackbox the source.
    if (currentRanges[url].length == 0) {
      delete currentRanges[url];
    }
  }
}

/*
 * Updates the all the state necessary for blackboxing
 *
 */
function updateBlackBoxState(state, blackboxSources) {
  const currentRanges = { ...state.blackboxedRanges };
  blackboxSources.map(({ source, shouldBlackBox, ranges }) =>
    updateBlackboxRangesForSourceUrl(
      currentRanges,
      source.url,
      shouldBlackBox,
      ranges
    )
  );
  return { ...state, blackboxedRanges: currentRanges };
}

export default update;
