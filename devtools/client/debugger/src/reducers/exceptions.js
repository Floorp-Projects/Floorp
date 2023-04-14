/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Exceptions reducer
 * @module reducers/exceptionss
 */

export function initialExceptionsState() {
  return {
    // Store exception objects created by actions/exceptions.js's addExceptionFromResources()
    // This is keyed by source actor id, and values are arrays of exception objects.
    mutableExceptionsMap: new Map(),
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
  const { sourceActorId } = exception;

  let exceptions = state.mutableExceptionsMap.get(sourceActorId);
  if (!exceptions) {
    exceptions = [];
    state.mutableExceptionsMap.set(sourceActorId, exceptions);
  }

  // As these arrays are only used by getSelectedSourceExceptions selector method,
  // which coalesce multiple arrays and always return new array instance,
  // it isn't important to clone these array in case of modification.
  exceptions.push(exception);

  return {
    ...state,
  };
}

export default update;
