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
  getItem,
  createFieldByIDGetter,
  createFieldReducer,
  type ResourceState,
  type FieldReducer,
  type FieldByIDGetter
} from "../utils/resource";

export opaque type SourceActorId = string;
export type SourceActor = {|
  id: SourceActorId,
  actor: string,
  thread: ThreadId,
  source: SourceId,

  isBlackBoxed: boolean,

  // The URL of the sourcemap for this source if there is one.
  sourceMapURL: string | null,

  // The URL of the actor itself. If the source was from an "eval" or other
  // string-based source, this will not be known.
  url: string | null,

  // If this script was introduced by an eval, this will be the URL of the
  // script that triggered the evaluation.
  introductionUrl: string | null,

  // The debugger's Debugger.Source API provides type information for the
  // cause of this source's creation.
  introductionType: string | null
|};

type SourceActorResource = {|
  item: SourceActor
|};
export type SourceActorsState = ResourceState<SourceActorResource>;
export type SourceActorOuterState = { sourceActors: SourceActorsState };

const initial: SourceActorsState = createInitial({
  item: {}
});

export default function update(
  state: SourceActorsState = initial,
  action: Action
): SourceActorsState {
  switch (action.type) {
    case "INSERT_SOURCE_ACTORS": {
      const { items } = action;
      state = insertResources(state, items.map(item => ({ item })));
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
  return getItem(state.sourceActors, id);
}

/**
 * Get all of the source actors for a set of IDs. Caches based on the identity
 * of "ids" when possible.
 */
const getSourceActorsById: FieldByIDGetter<
  SourceActorResource,
  "item"
> = createFieldByIDGetter("item");
export function getSourceActors(
  state: SourceActorOuterState,
  ids: Array<SourceActorId>
): Array<SourceActor> {
  return getSourceActorsById(state.sourceActors, ids);
}

const getSourcesByThreadID: FieldReducer<
  SourceActorResource,
  { [ThreadId]: Array<SourceActor> }
> = createFieldReducer(
  "item",
  (acc, item, id) => {
    acc[item.thread] = acc[item.thread] || [];
    acc[item.thread].push(item);
    return acc;
  },
  () => ({})
);
export function getSourceActorsForThread(
  state: SourceActorOuterState,
  ids: ThreadId | Array<ThreadId>
): Array<SourceActor> {
  const sourcesByThread = getSourcesByThreadID(state.sourceActors);

  let sources = [];
  for (const id of Array.isArray(ids) ? ids : [ids]) {
    sources = sources.concat(sourcesByThread[id] || []);
  }
  return sources;
}

const getThreadsBySourceObject: FieldReducer<
  SourceActorResource,
  { [SourceId]: Array<ThreadId> }
> = createFieldReducer(
  "item",
  (acc, item, id) => {
    let sourceThreads = acc[item.source];
    if (!sourceThreads) {
      sourceThreads = [];
      acc[item.source] = sourceThreads;
    }

    sourceThreads.push(item.thread);
    return acc;
  },
  () => ({})
);
export function getThreadsBySource(
  state: SourceActorOuterState
): { [SourceId]: Array<ThreadId> } {
  return getThreadsBySourceObject(state.sourceActors);
}
