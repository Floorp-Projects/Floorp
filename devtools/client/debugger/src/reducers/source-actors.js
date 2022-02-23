/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { asSettled } from "../utils/async-value";
import {
  createInitial,
  insertResources,
  updateResources,
  removeResources,
  hasResource,
  getResource,
  getMappedResource,
  makeWeakQuery,
  makeIdQuery,
  makeReduceAllQuery,
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

export function resourceAsSourceActor({
  breakpointPositions,
  breakableLines,
  ...sourceActor
}) {
  return sourceActor;
}

export function hasSourceActor(state, id) {
  return hasResource(state.sourceActors, id);
}

export function getSourceActor(state, id) {
  return getMappedResource(state.sourceActors, id, resourceAsSourceActor);
}

/**
 * Get all of the source actors for a set of IDs. Caches based on the identity
 * of "ids" when possible.
 */
const querySourceActorsById = makeIdQuery(resourceAsSourceActor);

export function getSourceActors(state, ids) {
  return querySourceActorsById(state.sourceActors, ids);
}

const querySourcesByThreadID = makeReduceAllQuery(
  resourceAsSourceActor,
  actors => {
    return actors.reduce((acc, actor) => {
      acc[actor.thread] = acc[actor.thread] || [];
      acc[actor.thread].push(actor);
      return acc;
    }, {});
  }
);
export function getSourceActorsForThread(state, ids) {
  const sourcesByThread = querySourcesByThreadID(state.sourceActors);

  let sources = [];
  for (const id of Array.isArray(ids) ? ids : [ids]) {
    sources = sources.concat(sourcesByThread[id] || []);
  }
  return sources;
}

const queryThreadsBySourceObject = makeReduceAllQuery(
  actor => ({ thread: actor.thread, source: actor.source }),
  actors =>
    actors.reduce((acc, { source, thread }) => {
      let sourceThreads = acc[source];
      if (!sourceThreads) {
        sourceThreads = [];
        acc[source] = sourceThreads;
      }

      sourceThreads.push(thread);
      return acc;
    }, {})
);

export function getAllThreadsBySource(state) {
  return queryThreadsBySourceObject(state.sourceActors);
}

export function getSourceActorBreakableLines(state, id) {
  const { breakableLines } = getResource(state.sourceActors, id);

  return asSettled(breakableLines);
}

export function getSourceActorBreakpointColumns(state, id, line) {
  const { breakpointPositions } = getResource(state.sourceActors, id);

  return asSettled(breakpointPositions.get(line) || null);
}

export const getBreakableLinesForSourceActors = makeWeakQuery({
  filter: (state, ids) => ids,
  map: ({ breakableLines }) => breakableLines,
  reduce: items =>
    Array.from(
      items.reduce((acc, item) => {
        if (item && item.state === "fulfilled") {
          acc = acc.concat(item.value);
        }
        return acc;
      }, [])
    ),
});
