/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Exceptions reducer
 * @module reducers/exceptionss
 */

export function initialExceptionsState() {
  return {
    exceptions: {},
  };
}

function update(state = initialExceptionsState(), action) {
  switch (action.type) {
    case "ADD_EXCEPTION":
      return updateExceptions(state, action);
  }
  return state;
}

function updateExceptions(state, action) {
  const { exception } = action;
  const sourceActorId = exception.sourceActorId;

  if (state.exceptions[sourceActorId]) {
    const sourceExceptions = state.exceptions[sourceActorId];
    return {
      ...state,
      exceptions: {
        ...state.exceptions,
        [sourceActorId]: [...sourceExceptions, exception],
      },
    };
  }
  return {
    ...state,
    exceptions: {
      ...state.exceptions,
      [sourceActorId]: [exception],
    },
  };
}

export default update;
