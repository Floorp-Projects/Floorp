/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { asyncActionAsValue } from "../actions/utils/middleware/promise";

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

    // Map(Source Actor ID: string => Breakable lines: object)
    // Breakable lines object is of the form: { state: <"pending"|"fulfilled">, value: Array<Number> }
    // The array is the list of all lines where breakpoints can be set
    mutableBreakableLines: new Map(),
  };
}

export const initial = initialSourceActorsState();

export default function update(state = initialSourceActorsState(), action) {
  switch (action.type) {
    case "INSERT_SOURCE_ACTORS": {
      for (const sourceActor of action.sourceActors) {
        state.mutableSourceActors.set(sourceActor.id, sourceActor);
      }
      return {
        ...state,
      };
    }

    case "NAVIGATE": {
      state = initialSourceActorsState();
      break;
    }

    case "REMOVE_THREAD": {
      for (const sourceActor of state.mutableSourceActors.values()) {
        if (sourceActor.thread == action.threadActorID) {
          state.mutableSourceActors.delete(sourceActor.id);
        }
      }
      return {
        ...state,
      };
    }

    case "SET_SOURCE_ACTOR_BREAKABLE_LINES":
      return updateBreakableLines(state, action);

    case "CLEAR_SOURCE_ACTOR_MAP_URL":
      return clearSourceActorMapURL(state, action.sourceActorId);
  }

  return state;
}

function clearSourceActorMapURL(state, sourceActorId) {
  const existingSourceActor = state.mutableSourceActors.get(sourceActorId);
  if (!existingSourceActor) {
    return state;
  }

  // /!\ We end up mutating sourceActor objects here /!\
  // The `sourceMapURL` attribute isn't reliable and we must query the selectors
  // each time we try to interpret its value via sourceActor.sourceMapURL!
  //
  // We should probably move this attribute into a dedicated map,
  // and uncouple it from sourceActor object.
  state.mutableSourceActors.set(sourceActorId, {
    ...existingSourceActor,
    sourceMapURL: "",
  });

  return {
    ...state,
  };
}

function updateBreakableLines(state, action) {
  const value = asyncActionAsValue(action);
  const { sourceActorId } = action;

  // Ignore breakable lines for source actors that aren't/no longer registered
  if (!state.mutableSourceActors.has(sourceActorId)) {
    return state;
  }

  state.mutableBreakableLines.set(sourceActorId, value);
  return {
    ...state,
  };
}
