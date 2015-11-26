/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const initialState = {
  openRequests: []
};

function update(state = initialState, action, emitChange) {
  const { seqId } = action;

  if(seqId) {
    if(action.status === 'start') {
      state.openRequests.push(seqId);
    }
    else if(action.status === 'error' || action.status === 'done') {
      state.openRequests = state.openRequests.filter(id => id !== seqId);
    }

    emitChange('open-requests', state.openRequests);
  }

  return state;
}

module.exports = update;
