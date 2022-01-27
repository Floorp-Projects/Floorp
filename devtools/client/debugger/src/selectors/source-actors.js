/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { asSettled } from "../utils/async-value";
import {
  hasResource,
  getResource,
  getMappedResource,
  makeWeakQuery,
  makeIdQuery,
  makeReduceAllQuery,
} from "../utils/resource";

function resourceAsSourceActor({
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

// Used by sources selectors
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
// Used by threads selectors
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

// Used by threads selectors
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

// Used by sources selectors
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
