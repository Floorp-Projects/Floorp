/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Sources reducer
 * @module reducers/sources
 */

import { originalToGeneratedId } from "devtools/client/shared/source-map-loader/index";
import { prefs } from "../utils/prefs";
import { createPendingSelectedLocation } from "../utils/location";

export function initialSourcesState(state) {
  return {
    /**
     * All currently available sources.
     *
     * See create.js: `createSourceObject` method for the description of stored objects.
     */
    mutableSources: new Map(),

    /**
     * List of override objects whose sources texts have been locally overridden.
     *
     * Object { sourceUrl, path }
     */
    mutableOverrideSources: state?.mutableOverrideSources || new Map(),

    /**
     * All sources associated with a given URL. When using source maps, multiple
     * sources can have the same URL.
     *
     * Dictionary(url => array<source id>)
     */
    urls: {},

    /**
     * Map of the source id's to one or more related original source id's
     * Only generated sources which have related original sources will be maintained here.
     *
     * Dictionary(source id => array<Original Source ID>)
     */
    originalSources: {},

    /**
     * Mapping of source id's to one or more source-actor's.
     * Dictionary whose keys are source id's and values are arrays
     * made of all the related source-actor's.
     * Note: The source mapped here are only generated sources.
     *
     * "source" are the objects stored in this reducer, in the `sources` attribute.
     * "source-actor" are the objects stored in the "source-actors.js" reducer, in its `sourceActors` attribute.
     *
     * Dictionary(source id => array<Source Actor object>)
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
     *
     * See `createPendingSelectedLocation` for the definition of this object.
     */
    pendingSelectedLocation: prefs.pendingSelectedLocation,
  };
}

function update(state = initialSourcesState(), action) {
  switch (action.type) {
    case "ADD_SOURCES":
      return addSources(state, action.sources);

    case "ADD_ORIGINAL_SOURCES":
      return addSources(state, action.originalSources);

    case "INSERT_SOURCE_ACTORS":
      return insertSourceActors(state, action);

    case "SET_SELECTED_LOCATION": {
      let pendingSelectedLocation = null;

      if (action.source.url) {
        pendingSelectedLocation = createPendingSelectedLocation(
          action.location
        );
        prefs.pendingSelectedLocation = pendingSelectedLocation;
      }

      return {
        ...state,
        selectedLocation: action.location,
        pendingSelectedLocation,
      };
    }

    case "CLEAR_SELECTED_LOCATION": {
      const pendingSelectedLocation = { url: "" };
      prefs.pendingSelectedLocation = pendingSelectedLocation;

      return {
        ...state,
        selectedLocation: null,
        pendingSelectedLocation,
      };
    }

    case "SET_PENDING_SELECTED_LOCATION": {
      const pendingSelectedLocation = {
        url: action.url,
        line: action.line,
        column: action.column,
      };

      prefs.pendingSelectedLocation = pendingSelectedLocation;
      return { ...state, pendingSelectedLocation };
    }

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
      return removeSourcesAndActors(state, action);
    }

    case "SET_OVERRIDE": {
      state.mutableOverrideSources.set(action.url, action.path);
      return state;
    }

    case "REMOVE_OVERRIDE": {
      if (state.mutableOverrideSources.has(action.url)) {
        state.mutableOverrideSources.delete(action.url);
      }
      return state;
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

  for (const source of sources) {
    state.mutableSources.set(source.id, source);

    // Update the source url map
    const existing = state.urls[source.url] || [];
    if (!existing.includes(source.id)) {
      state.urls[source.url] = [...existing, source.id];
    }

    if (source.isOriginal) {
      const generatedSourceId = originalToGeneratedId(source.id);
      if (!state.originalSources[generatedSourceId]) {
        state.originalSources[generatedSourceId] = [];
      }
      state.originalSources[generatedSourceId].push(source.id);
    }
  }

  return state;
}

function removeSourcesAndActors(state, action) {
  state = {
    ...state,
    urls: { ...state.urls },
    actors: { ...state.actors },
    originalSources: { ...state.originalSources },
  };

  const { urls, mutableSources, originalSources, actors } = state;
  for (const removedSource of action.sources) {
    const sourceId = removedSource.id;

    // Clear the urls Map
    const sourceUrl = removedSource.url;
    if (sourceUrl) {
      let sourcesForUrl = urls[sourceUrl];
      if (sourcesForUrl) {
        sourcesForUrl = sourcesForUrl.filter(id => id !== sourceId);
        urls[sourceUrl] = sourcesForUrl;
        if (!sourcesForUrl.length) {
          delete urls[sourceUrl];
        }
      }
    }

    mutableSources.delete(sourceId);

    // Note that the caller of this method queried the reducer state
    // to aggregate the related original sources.
    // So if we were having related original sources, they will be
    // in `action.sources`.
    delete originalSources[sourceId];

    // If a source is removed, immediately remove all its related source actors.
    // It can speed-up the following for loop cleaning actors.
    delete actors[sourceId];
  }

  for (const removedActor of action.actors) {
    const sourceId = removedActor.source;
    const actorsForSource = actors[sourceId];
    // The entry might already have been cleared by the previous for..loop.
    if (!actorsForSource) {
      continue;
    }
    const idx = actorsForSource.indexOf(removedActor);
    if (idx != -1) {
      actorsForSource.splice(idx, 1);
      // Selectors are still expecting new array instances on any update
      actors[sourceId] = [...actorsForSource];
    }
    // Remove the entry in state.actors if there is no more actors for that source
    if (!actorsForSource.length) {
      delete actors[sourceId];
    }
  }

  return state;
}

function insertSourceActors(state, action) {
  const { sourceActors } = action;
  state = {
    ...state,
    actors: { ...state.actors },
  };

  // The `sourceActor` objects are defined from `newGeneratedSources` action:
  // https://searchfox.org/mozilla-central/rev/4646b826a25d3825cf209db890862b45fa09ffc3/devtools/client/debugger/src/actions/sources/newSources.js#300-314
  for (const sourceActor of sourceActors) {
    state.actors[sourceActor.source] = [
      ...(state.actors[sourceActor.source] || []),
      sourceActor,
    ];
  }

  const scriptActors = sourceActors.filter(
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
