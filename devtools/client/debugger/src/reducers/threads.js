/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Threads reducer
 * @module reducers/threads
 */

export function initialThreadsState() {
  return {
    threads: [],

    // List of thread actor IDs which are current tracing.
    // i.e. where JavaScript tracing is enabled.
    mutableTracingThreads: new Set(),
  };
}

export default function update(state = initialThreadsState(), action) {
  switch (action.type) {
    case "INSERT_THREAD":
      return {
        ...state,
        threads: [...state.threads, action.newThread],
      };

    case "REMOVE_THREAD":
      return {
        ...state,
        threads: state.threads.filter(
          thread => action.threadActorID != thread.actor
        ),
      };

    case "UPDATE_SERVICE_WORKER_STATUS":
      return {
        ...state,
        threads: state.threads.map(t => {
          if (t.actor == action.thread) {
            return { ...t, serviceWorkerStatus: action.status };
          }
          return t;
        }),
      };

    case "TRACING_TOGGLED":
      const { mutableTracingThreads } = state;
      const sizeBefore = mutableTracingThreads.size;
      if (action.enabled) {
        mutableTracingThreads.add(action.thread);
      } else {
        mutableTracingThreads.delete(action.thread);
      }
      // We may receive toggle events when we change the logging method
      // while we are already tracing, but the list of tracing thread stays the same.
      const changed = mutableTracingThreads.size != sizeBefore;
      if (changed) {
        return {
          ...state,
          mutableTracingThreads,
        };
      }
      return state;

    default:
      return state;
  }
}
