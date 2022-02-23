/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isOriginalId, originalToGeneratedId } from "devtools-source-map";
import { createSelector } from "reselect";
import { getSourceActorsForSource } from "../selectors/sources";

export const getThreads = createSelector(
  state => state.threads.threads,
  threads => threads.filter(thread => !isMainThread(thread))
);

export const getAllThreads = createSelector(
  getMainThread,
  getThreads,
  (mainThread, threads) => {
    const orderedThreads = Array.from(threads).sort((threadA, threadB) => {
      if (threadA.name === threadB.name) {
        return 0;
      }
      return threadA.name < threadB.name ? -1 : 1;
    });
    return [mainThread, ...orderedThreads].filter(Boolean);
  }
);

function isMainThread(thread) {
  return thread.isTopLevel;
}

export function getMainThread(state) {
  return state.threads.threads.find(isMainThread);
}

export function getDebuggeeUrl(state) {
  return getMainThread(state)?.url || "";
}

export function getThread(state, threadActor) {
  return getAllThreads(state).find(thread => thread.actor === threadActor);
}

/**
 * Find the thread for a specified source
 *
 * @param {Object} state
 * @param {String} sourceId
 * @return {Object} The thread object for the source.
 */
export function getThreadForSource(state, sourceId) {
  const actors = getSourceActorsForSource(
    state,
    isOriginalId(sourceId) ? originalToGeneratedId(sourceId) : sourceId
  );

  if (!actors || !actors.length) {
    console.error(`Error no source actors exist for source ${sourceId}`);
    return null;
  }
  return getThread(state, actors[0].thread);
}

// checks if a path begins with a thread actor
// e.g "server1.conn0.child1/workerTarget22/context1/dbg-workers.glitch.me"
export function startsWithThreadActor(state, path) {
  const threadActors = getAllThreads(state).map(t => t.actor);
  const match = path.match(new RegExp(`(${threadActors.join("|")})\/(.*)`));
  return match?.[1];
}
