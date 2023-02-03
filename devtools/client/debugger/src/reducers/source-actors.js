/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { asyncActionAsValue } from "../actions/utils/middleware/promise";

/**
 * This reducer stores the list of all source actors.
 * There is a one-one relationship with Source Actors from the server codebase,
 * as well as SOURCE Resources distributed by the ResourceCommand API.
 *
 * See create.js: `createSourceActor` for the shape of the source actor objects.
 * This reducer will append the following attributes:
 * - breakableLines: { state: <"pending"|"fulfilled">, value: Array<Number> }
 *   List of all lines where breakpoints can be set
 */
export const initial = new Map();

export default function update(state = initial, action) {
  switch (action.type) {
    case "INSERT_SOURCE_ACTORS": {
      const { items } = action;
      // The `item` objects are defined from create.js: `createSource` method.
      state = new Map(state);
      for (const sourceActor of items) {
        state.set(sourceActor.id, {
          ...sourceActor,
          breakableLines: null,
        });
      }
      break;
    }

    case "NAVIGATE": {
      state = initial;
      break;
    }

    case "REMOVE_THREAD": {
      state = new Map(state);
      for (const sourceActor of state.values()) {
        if (sourceActor.thread == action.threadActorID) {
          state.delete(sourceActor.id);
        }
      }
      break;
    }

    case "SET_SOURCE_ACTOR_BREAKABLE_LINES":
      state = updateBreakableLines(state, action);
      break;

    case "CLEAR_SOURCE_ACTOR_MAP_URL":
      state = clearSourceActorMapURL(state, action.id);
      break;
  }

  return state;
}

function clearSourceActorMapURL(state, id) {
  if (!state.has(id)) {
    return state;
  }

  const newMap = new Map(state);
  newMap.set(id, {
    ...state.get(id),
    sourceMapURL: "",
  });
  return newMap;
}

function updateBreakableLines(state, action) {
  const value = asyncActionAsValue(action);
  const { sourceId } = action;

  if (!state.has(sourceId)) {
    return state;
  }

  const newMap = new Map(state);
  newMap.set(sourceId, {
    ...state.get(sourceId),
    breakableLines: value,
  });
  return newMap;
}
