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

export const UNDEFINED_LOCATION = Symbol("Undefined location");
export const NO_LOCATION = Symbol("No location");

export function initialSourcesState(state) {
  /* eslint sort-keys: "error" */
  return {
    /**
     * List of all breakpoint positions for all sources (generated and original).
     * Map of source id (string) to dictionary object whose keys are line numbers
     * and values of array of positions.
     * A position is an object made with two attributes:
     * location and generatedLocation. Both refering to breakpoint positions
     * in original and generated sources.
     * In case of generated source, the two location will be the same.
     *
     * Map(source id => Dictionary(int => array<Position>))
     */
    mutableBreakpointPositions: new Map(),

    /**
     * List of all breakable lines for original sources only.
     *
     * Map(source id => array<int : breakable line numbers>)
     */
    mutableOriginalBreakableLines: new Map(),

    /**
     * Map of the source id's to one or more related original source id's
     * Only generated sources which have related original sources will be maintained here.
     *
     * Map(source id => array<Original Source ID>)
     */
    mutableOriginalSources: new Map(),

    /**
     * List of override objects whose sources texts have been locally overridden.
     *
     * Object { sourceUrl, path }
     */
    mutableOverrideSources: state?.mutableOverrideSources || new Map(),

    /**
     * Mapping of source id's to one or more source-actor's.
     * Dictionary whose keys are source id's and values are arrays
     * made of all the related source-actor's.
     * Note: The source mapped here are only generated sources.
     *
     * "source" are the objects stored in this reducer, in the `sources` attribute.
     * "source-actor" are the objects stored in the "source-actors.js" reducer, in its `sourceActors` attribute.
     *
     * Map(source id => array<Source Actor object>)
     */
    mutableSourceActors: new Map(),

    /**
     * All currently available sources.
     *
     * See create.js: `createSourceObject` method for the description of stored objects.
     */
    mutableSources: new Map(),

    /**
     * All sources associated with a given URL. When using source maps, multiple
     * sources can have the same URL.
     *
     * Map(url => array<source>)
     */
    mutableSourcesPerUrl: new Map(),

    /**
     * When we want to select a source that isn't available yet, use this.
     * The location object should have a url attribute instead of a sourceId.
     *
     * See `createPendingSelectedLocation` for the definition of this object.
     */
    pendingSelectedLocation: prefs.pendingSelectedLocation,

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
     * When selectedLocation refers to a generated source mapping to an original source
     * via a source-map, refers to the related original location.
     *
     * This is UNDEFINED_LOCATION by default and will switch to NO_LOCATION asynchronously after location
     * selection if there is no valid original location to map to.
     */
    selectedOriginalLocation: UNDEFINED_LOCATION,

    /**
     * By default, if we have a source-mapped source, we would automatically try
     * to select and show the content of the original source. But, if we explicitly
     * select a generated source, we remember this choice. That, until we explicitly
     * select an original source.
     * Note that selections related to non-source-mapped sources should never
     * change this setting.
     */
    shouldSelectOriginalLocation: true,
  };
  /* eslint-disable sort-keys */
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

      if (action.location.source.url) {
        pendingSelectedLocation = createPendingSelectedLocation(
          action.location
        );
        prefs.pendingSelectedLocation = pendingSelectedLocation;
      }

      return {
        ...state,
        selectedLocation: action.location,
        selectedOriginalLocation: UNDEFINED_LOCATION,
        pendingSelectedLocation,
        shouldSelectOriginalLocation: action.shouldSelectOriginalLocation,
      };
    }

    case "CLEAR_SELECTED_LOCATION": {
      const pendingSelectedLocation = { url: "" };
      prefs.pendingSelectedLocation = pendingSelectedLocation;

      return {
        ...state,
        selectedLocation: null,
        selectedOriginalLocation: UNDEFINED_LOCATION,
        pendingSelectedLocation,
      };
    }

    case "SET_ORIGINAL_SELECTED_LOCATION": {
      if (action.location != state.selectedLocation) {
        return state;
      }
      return {
        ...state,
        selectedOriginalLocation: action.originalLocation,
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
      state.mutableOriginalBreakableLines.set(
        action.source.id,
        action.breakableLines
      );

      return {
        ...state,
      };
    }

    case "ADD_BREAKPOINT_POSITIONS": {
      // Merge existing and new reported position if some where already stored
      let positions = state.mutableBreakpointPositions.get(action.source.id);
      if (positions) {
        positions = { ...positions, ...action.positions };
      } else {
        positions = action.positions;
      }

      state.mutableBreakpointPositions.set(action.source.id, positions);

      return {
        ...state,
      };
    }

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
 * - Add the source URL to the source url map
 */
function addSources(state, sources) {
  for (const source of sources) {
    state.mutableSources.set(source.id, source);

    // Update the source url map
    const existing = state.mutableSourcesPerUrl.get(source.url);
    if (existing) {
      // We never return this array from selectors as-is,
      // we either return the first entry or lookup for a precise entry
      // so we can mutate it.
      existing.push(source);
    } else {
      state.mutableSourcesPerUrl.set(source.url, [source]);
    }

    // In case of original source, maintain the mapping of generated source to original sources map.
    if (source.isOriginal) {
      const generatedSourceId = originalToGeneratedId(source.id);
      let originalSourceIds =
        state.mutableOriginalSources.get(generatedSourceId);
      if (!originalSourceIds) {
        originalSourceIds = [];
        state.mutableOriginalSources.set(generatedSourceId, originalSourceIds);
      }
      // We never return this array out of selectors, so mutate the list
      originalSourceIds.push(source.id);
    }
  }

  return { ...state };
}

function removeSourcesAndActors(state, action) {
  const {
    mutableSourcesPerUrl,
    mutableSources,
    mutableOriginalSources,
    mutableSourceActors,
    mutableOriginalBreakableLines,
    mutableBreakpointPositions,
  } = state;

  const newState = { ...state };

  for (const removedSource of action.sources) {
    const sourceId = removedSource.id;

    // Clear the urls Map
    const sourceUrl = removedSource.url;
    if (sourceUrl) {
      const sourcesForSameUrl = (
        mutableSourcesPerUrl.get(sourceUrl) || []
      ).filter(s => s != removedSource);
      if (!sourcesForSameUrl.length) {
        // All sources with this URL have been removed
        mutableSourcesPerUrl.delete(sourceUrl);
      } else {
        // There are other sources still alive with the same URL
        mutableSourcesPerUrl.set(sourceUrl, sourcesForSameUrl);
      }
    }

    mutableSources.delete(sourceId);

    // Note that the caller of this method queried the reducer state
    // to aggregate the related original sources.
    // So if we were having related original sources, they will be
    // in `action.sources`.
    mutableOriginalSources.delete(sourceId);

    // If a source is removed, immediately remove all its related source actors.
    // It can speed-up the following for loop cleaning actors.
    mutableSourceActors.delete(sourceId);

    if (removedSource.isOriginal) {
      mutableOriginalBreakableLines.delete(sourceId);
    }

    mutableBreakpointPositions.delete(sourceId);

    if (newState.selectedLocation?.source == removedSource) {
      newState.selectedLocation = null;
      newState.selectedOriginalLocation = UNDEFINED_LOCATION;
    }
  }

  for (const removedActor of action.actors) {
    const sourceId = removedActor.source;
    const actorsForSource = mutableSourceActors.get(sourceId);
    // actors may have already been cleared by the previous for..loop
    if (!actorsForSource) {
      continue;
    }
    const idx = actorsForSource.indexOf(removedActor);
    if (idx != -1) {
      actorsForSource.splice(idx, 1);
      // While the Map is mutable, we expect new array instance on each new change
      mutableSourceActors.set(sourceId, [...actorsForSource]);
    }

    // Remove the entry in the Map if there is no more actors for that source
    if (!actorsForSource.length) {
      mutableSourceActors.delete(sourceId);
    }

    if (newState.selectedLocation?.sourceActor == removedActor) {
      newState.selectedLocation = null;
      newState.selectedOriginalLocation = UNDEFINED_LOCATION;
    }
  }

  return newState;
}

function insertSourceActors(state, action) {
  const { sourceActors } = action;

  const { mutableSourceActors } = state;
  // The `sourceActor` objects are defined from `newGeneratedSources` action:
  // https://searchfox.org/mozilla-central/rev/4646b826a25d3825cf209db890862b45fa09ffc3/devtools/client/debugger/src/actions/sources/newSources.js#300-314
  for (const sourceActor of sourceActors) {
    const sourceId = sourceActor.source;
    // We always clone the array of source actors as we return it from selectors.
    // So the map is mutable, but its values are considered immutable and will change
    // anytime there is a new actor added per source ID.
    const existing = mutableSourceActors.get(sourceId);
    if (existing) {
      mutableSourceActors.set(sourceId, [...existing, sourceActor]);
    } else {
      mutableSourceActors.set(sourceId, [sourceActor]);
    }
  }

  const scriptActors = sourceActors.filter(
    item => item.introductionType === "scriptElement"
  );
  if (scriptActors.length) {
    // If new HTML sources are being added, we need to clear the breakpoint
    // positions since the new source is a <script> with new breakpoints.
    for (const { source } of scriptActors) {
      state.mutableBreakpointPositions.delete(source);
    }
  }

  return { ...state };
}

export default update;
