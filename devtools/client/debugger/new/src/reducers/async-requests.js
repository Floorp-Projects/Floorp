/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Async request reducer
 * @module reducers/async-request
 */

const initialAsyncRequestState = [];

function update(state: string[] = initialAsyncRequestState, action: any) {
  const { seqId } = action;

  if (action.type === "NAVIGATE") {
    return initialAsyncRequestState;
  } else if (seqId) {
    let newState;
    if (action.status === "start") {
      newState = [...state, seqId];
    } else if (action.status === "error" || action.status === "done") {
      newState = (state.filter(id => id !== seqId): string[]);
    }

    return newState;
  }

  return state;
}

export default update;
