/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This reducer stores the list of all source actors as well their breakable lines.
 *
 * There is a one-one relationship with Source Actors from the server codebase,
 * as well as SOURCE Resources distributed by the ResourceCommand API.
 *
 */
function initialSourceActorsState() {
  return {
    // Map(Source Actor ID: string => SourceActor: object)
    // See create.js: `createSourceActor` for the shape of the source actor objects.
    mutableSourceActors: new Map(),

    // Map(Source Actor ID: string => Breakable lines: Array<Number>)
    // The array is the list of all lines where breakpoints can be set
    mutableBreakableLines: new Map(),

    // Set(Source Actor ID: string)
    // List of all IDs of source actor which have a valid related source map / original source.
    // The SourceActor object may have a sourceMapURL attribute set,
    // but this may be invalid. The source map URL or source map file content may be invalid.
    // In these scenarios we will remove the source actor from this set.
    mutableSourceActorsWithSourceMap: new Set(),

    // Map(Source Actor ID: string => string)
    // Store the exception message when processing the sourceMapURL field of the source actor.
    mutableSourceMapErrors: new Map(),

    // Map(Source Actor ID: string => string)
    // When a bundle has a functional sourcemap, reports the resolved source map URL.
    mutableResolvedSourceMapURL: new Map(),
  };
}

export const initial = initialSourceActorsState();

export default function update(state = initialSourceActorsState(), action) {
  switch (action.type) {
    case "INSERT_SOURCE_ACTORS": {
      for (const sourceActor of action.sourceActors) {
        state.mutableSourceActors.set(sourceActor.id, sourceActor);

        // If the sourceMapURL attribute is set, consider that it is valid.
        // But this may be revised later and removed from this Set.
        if (sourceActor.sourceMapURL) {
          state.mutableSourceActorsWithSourceMap.add(sourceActor.id);
        }
      }
      return {
        ...state,
      };
    }

    case "REMOVE_THREAD": {
      for (const sourceActor of state.mutableSourceActors.values()) {
        if (sourceActor.thread == action.threadActorID) {
          state.mutableSourceActors.delete(sourceActor.id);
          state.mutableBreakableLines.delete(sourceActor.id);
          state.mutableSourceActorsWithSourceMap.delete(sourceActor.id);
        }
      }
      return {
        ...state,
      };
    }

    case "SET_SOURCE_ACTOR_BREAKABLE_LINES":
      state.mutableBreakableLines.set(
        action.sourceActor.id,
        action.breakableLines
      );

      return {
        ...state,
      };

    case "CLEAR_SOURCE_ACTOR_MAP_URL":
      if (
        state.mutableSourceActorsWithSourceMap.delete(action.sourceActor.id)
      ) {
        return {
          ...state,
        };
      }
      return state;

    case "SOURCE_MAP_ERROR": {
      state.mutableSourceMapErrors.set(
        action.sourceActor.id,
        action.errorMessage
      );
      return { ...state };
    }

    case "RESOLVED_SOURCEMAP_URL": {
      state.mutableResolvedSourceMapURL.set(
        action.sourceActor.id,
        action.resolvedSourceMapURL
      );
      return { ...state };
    }
  }

  return state;
}
