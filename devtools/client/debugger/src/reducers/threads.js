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
    isWebExtension: false,
  };
}

export default function update(state = initialThreadsState(), action) {
  switch (action.type) {
    case "CONNECT":
      return {
        ...state,
        isWebExtension: action.isWebExtension,
      };
    case "INSERT_THREAD":
      return {
        ...state,
        threads: [...state.threads, action.newThread],
      };

    case "REMOVE_THREAD":
      const { oldThread } = action;
      return {
        ...state,
        threads: state.threads.filter(
          thread => oldThread.actor != thread.actor
        ),
      };
    case "UPDATE_SERVICE_WORKER_STATUS":
      const { thread, status } = action;
      return {
        ...state,
        threads: state.threads.map(t => {
          if (t.actor == thread) {
            return { ...t, serviceWorkerStatus: status };
          }
          return t;
        }),
      };

    default:
      return state;
  }
}
