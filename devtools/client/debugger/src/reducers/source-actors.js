/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Action } from "../actions/types";
import type { SourceId, ThreadId, URL } from "../types";
import {
  asSettled,
  type AsyncValue,
  type SettledValue,
} from "../utils/async-value";
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
  type Resource,
  type ResourceState,
  type WeakQuery,
  type IdQuery,
  type ReduceAllQuery,
} from "../utils/resource";

import { asyncActionAsValue } from "../actions/utils/middleware/promise";
import type {
  SourceActorBreakpointColumnsAction,
  SourceActorBreakableLinesAction,
} from "../actions/types/SourceActorAction";

export opaque type SourceActorId: string = string;
export type SourceActor = {|
  +id: SourceActorId,
  +actor: string,
  +thread: ThreadId,
  +source: SourceId,

  +isBlackBoxed: boolean,

  // The URL of the sourcemap for this source if there is one.
  +sourceMapURL: URL | null,

  // The URL of the actor itself. If the source was from an "eval" or other
  // string-based source, this will not be known.
  +url: URL | null,

  // If this script was introduced by an eval, this will be the URL of the
  // script that triggered the evaluation.
  +introductionUrl: URL | null,

  // The debugger's Debugger.Source API provides type information for the
  // cause of this source's creation.
  +introductionType: string | null,
|};

type SourceActorResource = Resource<{
  ...SourceActor,

  // The list of breakpoint positions on each line of the file.
  breakpointPositions: Map<number, AsyncValue<Array<number>>>,

  // The list of lines that contain breakpoints.
  breakableLines: AsyncValue<Array<number>> | null,
}>;
export type SourceActorsState = ResourceState<SourceActorResource>;
export type SourceActorOuterState = { sourceActors: SourceActorsState };

const initial: SourceActorsState = createInitial();

export default function update(
  state: SourceActorsState = initial,
  action: Action
): SourceActorsState {
  switch (action.type) {
    case "INSERT_SOURCE_ACTORS": {
      const { items } = action;
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
      const { items } = action;
      state = removeResources(state, items);
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

function clearSourceActorMapURL(
  state: SourceActorsState,
  id: SourceActorId
): SourceActorsState {
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

function updateBreakpointColumns(
  state: SourceActorsState,
  action: SourceActorBreakpointColumnsAction
): SourceActorsState {
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

function updateBreakableLines(
  state: SourceActorsState,
  action: SourceActorBreakableLinesAction
): SourceActorsState {
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
}: SourceActorResource): SourceActor {
  return sourceActor;
}

// Because we are using an opaque type for our source actor IDs, these
// functions are required to convert back and forth in order to get a string
// version of the IDs. That should be super rarely used, but it means that
// we can very easily see where we're relying on the string version of IDs.
export function stringToSourceActorId(s: string): SourceActorId {
  return s;
}

export function hasSourceActor(
  state: SourceActorOuterState,
  id: SourceActorId
): boolean {
  return hasResource(state.sourceActors, id);
}

export function getSourceActor(
  state: SourceActorOuterState,
  id: SourceActorId
): SourceActor {
  return getMappedResource(state.sourceActors, id, resourceAsSourceActor);
}

/**
 * Get all of the source actors for a set of IDs. Caches based on the identity
 * of "ids" when possible.
 */
const querySourceActorsById: IdQuery<
  SourceActorResource,
  SourceActor
> = makeIdQuery(resourceAsSourceActor);

export function getSourceActors(
  state: SourceActorOuterState,
  ids: Array<SourceActorId>
): Array<SourceActor> {
  return querySourceActorsById(state.sourceActors, ids);
}

const querySourcesByThreadID: ReduceAllQuery<
  SourceActorResource,
  { [ThreadId]: Array<SourceActor> }
> = makeReduceAllQuery(resourceAsSourceActor, actors => {
  return actors.reduce((acc, actor) => {
    acc[actor.thread] = acc[actor.thread] || [];
    acc[actor.thread].push(actor);
    return acc;
  }, {});
});
export function getSourceActorsForThread(
  state: SourceActorOuterState,
  ids: ThreadId | Array<ThreadId>
): Array<SourceActor> {
  const sourcesByThread = querySourcesByThreadID(state.sourceActors);

  let sources = [];
  for (const id of Array.isArray(ids) ? ids : [ids]) {
    sources = sources.concat(sourcesByThread[id] || []);
  }
  return sources;
}

const queryThreadsBySourceObject: ReduceAllQuery<
  SourceActorResource,
  { [SourceId]: Array<ThreadId> }
> = makeReduceAllQuery(
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

export function getAllThreadsBySource(
  state: SourceActorOuterState
): { [SourceId]: Array<ThreadId> } {
  return queryThreadsBySourceObject(state.sourceActors);
}

export function getSourceActorBreakableLines(
  state: SourceActorOuterState,
  id: SourceActorId
): SettledValue<Array<number>> | null {
  const { breakableLines } = getResource(state.sourceActors, id);

  return asSettled(breakableLines);
}

export function getSourceActorBreakpointColumns(
  state: SourceActorOuterState,
  id: SourceActorId,
  line: number
): SettledValue<Array<number>> | null {
  const { breakpointPositions } = getResource(state.sourceActors, id);

  return asSettled(breakpointPositions.get(line) || null);
}

export const getBreakableLinesForSourceActors: WeakQuery<
  SourceActorResource,
  Array<SourceActorId>,
  Array<number>
> = makeWeakQuery({
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
