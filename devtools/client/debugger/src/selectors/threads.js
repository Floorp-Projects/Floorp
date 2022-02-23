/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";

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

// checks if a path begins with a thread actor
// e.g "server1.conn0.child1/workerTarget22/context1/dbg-workers.glitch.me"
export function startsWithThreadActor(state, path) {
  const threadActors = getAllThreads(state).map(t => t.actor);
  const match = path.match(new RegExp(`(${threadActors.join("|")})\/(.*)`));
  return match?.[1];
}
