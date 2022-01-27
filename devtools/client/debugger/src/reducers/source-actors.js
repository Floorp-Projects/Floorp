/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  createInitial,
  insertResources,
  updateResources,
  removeResources,
  hasResource,
  getResource,
} from "../utils/resource";

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
 * - breakpointPositions: Map<line, { state: <"pending"|"fulfilled">, value: Array<Number> }>
 *   Map of the column positions per line where breakpoints can be set
 */
export const initial = createInitial();

export default function update(state = initial, action) {
  switch (action.type) {
    case "INSERT_SOURCE_ACTORS": {
      const { items } = action;
      // The `item` objects are defined from create.js: `createSource` method.
      state = insertResources(
        state,
        items.map(item => ({
          ...item,
          breakpointPositions: new Map(),
          breakableLines: null,
        }))
      );
      break;
    }
    case "REMOVE_SOURCE_ACTORS": {
      state = removeResources(state, action.items);
      break;
    }

    case "NAVIGATE": {
      state = initial;
      break;
    }

    case "SET_SOURCE_ACTOR_BREAKPOINT_COLUMNS":
      state = updateBreakpointColumns(state, action);
      break;

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
  if (!hasResource(state, id)) {
    return state;
  }

  return updateResources(state, [
    {
      id,
      sourceMapURL: "",
    },
  ]);
}

function updateBreakpointColumns(state, action) {
  const { sourceId, line } = action;
  const value = asyncActionAsValue(action);

  if (!hasResource(state, sourceId)) {
    return state;
  }

  const breakpointPositions = new Map(
    getResource(state, sourceId).breakpointPositions
  );
  breakpointPositions.set(line, value);

  return updateResources(state, [{ id: sourceId, breakpointPositions }]);
}

function updateBreakableLines(state, action) {
  const value = asyncActionAsValue(action);
  const { sourceId } = action;

  if (!hasResource(state, sourceId)) {
    return state;
  }

  return updateResources(state, [{ id: sourceId, breakableLines: value }]);
}
