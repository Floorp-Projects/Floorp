/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Sources reducer
 * @module reducers/sources
 */

import { prefs } from "../utils/prefs";

export function initialSourcesState(state) {
  return {
    /**
     * All currently available sources.
     *
     * See create.js: `createSourceObject` method for the description of stored objects.
     */
    sources: new Map(),

    /**
     * All sources associated with a given URL. When using source maps, multiple
     * sources can have the same URL.
     *
     * Dictionary(url => array<source id>)
     */
    urls: {},

    /**
     * Mapping of source id's to one or more source-actor id's.
     * Dictionary whose keys are source id's and values are arrays
     * made of all the related source-actor id's.
     *
     * "source" are the objects stored in this reducer, in the `sources` attribute.
     * "source-actor" are the objects stored in the "source-actors.js" reducer, in its `sourceActors` attribute.
     *
     * Dictionary(source id => array<Object(id: SourceActor Id, thread: Thread Actor ID)>)
     */
    actors: {},

    breakpointPositions: {},
    breakableLines: {},

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
  };
}

function update(state = initialSourcesState(), action) {
  let location = null;

  switch (action.type) {
    case "ADD_SOURCES":
      return addSources(state, action.sources);

    case "INSERT_SOURCE_ACTORS":
      return insertSourceActors(state, action);

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
      return initialSourcesState(state);

    case "REMOVE_THREAD": {
      return removeSourcesAndActors(state, action.threadActorID);
    }
  }

  return state;
}

/*
 * Add sources to the sources store
 * - Add the source to the sources store
 * - Add the source URL to the urls map
 */
function addSources(state, sources) {
  state = {
    ...state,
    urls: { ...state.urls },
  };

  const newSourceMap = new Map(state.sources);
  for (const source of sources) {
    newSourceMap.set(source.id, source);

    // Update the source url map
    const existing = state.urls[source.url] || [];
    if (!existing.includes(source.id)) {
      state.urls[source.url] = [...existing, source.id];
    }
  }
  state.sources = newSourceMap;

  return state;
}

function removeSourcesAndActors(state, threadActorID) {
  state = {
    ...state,
    actors: { ...state.actors },
    urls: { ...state.urls },
  };

  const newSourceMap = new Map(state.sources);
  for (const sourceId in state.actors) {
    let i = state.actors[sourceId].length;
    while (i--) {
      // delete the source actors which belong to the
      // specified thread.
      if (state.actors[sourceId][i].thread == threadActorID) {
        state.actors[sourceId].splice(i, 1);
      }
    }
    // Delete the source only if all its actors belong to
    // the same thread.
    if (!state.actors[sourceId].length) {
      delete state.actors[sourceId];

      const source = newSourceMap.get(sourceId);
      if (source.url) {
        // urls
        if (state.urls[source.url]) {
          state.urls[source.url] = state.urls[source.url].filter(
            id => id !== source.id
          );
        }
        if (state.urls[source.url]?.length == 0) {
          delete state.urls[source.url];
        }
      }

      newSourceMap.delete(sourceId);
    }
  }
  state.sources = newSourceMap;
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
      { id: sourceActor.id, thread: sourceActor.thread },
    ];
  }

  const scriptActors = items.filter(
    item => item.introductionType === "scriptElement"
  );
  if (scriptActors.length) {
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

export default update;
