/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Action } from "../actions/types";
import type { SourceId, ThreadId } from "../types";
import {
  createInitial,
  insertResources,
  removeResources,
  hasResource,
  getResource,
  makeIdQuery,
  makeReduceAllQuery,
  type Resource,
  type ResourceState,
  type IdQuery,
  type ReduceAllQuery,
} from "../utils/resource";

export opaque type SourceActorId: string = string;
export type SourceActor = {|
  +id: SourceActorId,
  +actor: string,
  +thread: ThreadId,
  +source: SourceId,

  +isBlackBoxed: boolean,

  // The URL of the sourcemap for this source if there is one.
  +sourceMapURL: string | null,

  // The URL of the actor itself. If the source was from an "eval" or other
  // string-based source, this will not be known.
  +url: string | null,

  // If this script was introduced by an eval, this will be the URL of the
  // script that triggered the evaluation.
  +introductionUrl: string | null,

  // The debugger's Debugger.Source API provides type information for the
  // cause of this source's creation.
  +introductionType: string | null,
|};

type SourceActorResource = Resource<{
  ...SourceActor,
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
      state = insertResources(state, items);
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
  }

  return state;
}

export function resourceAsSourceActor(r: SourceActorResource): SourceActor {
  return r;
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
  return getResource(state.sourceActors, id);
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

export function getThreadsBySource(
  state: SourceActorOuterState
): { [SourceId]: Array<ThreadId> } {
  return queryThreadsBySourceObject(state.sourceActors);
}
