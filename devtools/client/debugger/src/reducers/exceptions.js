/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Exceptions reducer
 * @module reducers/exceptionss
 */

import { createSelector } from "reselect";
import { getSelectedSource, getSourceActorsForSource } from "../selectors";

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

// Selectors
export function getExceptionsMap(state) {
  return state.exceptions.exceptions;
}

export const getSelectedSourceExceptions = createSelector(
  getSelectedSourceActors,
  getExceptionsMap,
  (sourceActors, exceptions) => {
    const sourceExceptions = [];

    sourceActors.forEach(sourceActor => {
      const actorId = sourceActor.id;

      if (exceptions[actorId]) {
        sourceExceptions.push(...exceptions[actorId]);
      }
    });

    return sourceExceptions;
  }
);

function getSelectedSourceActors(state) {
  const selectedSource = getSelectedSource(state);
  if (!selectedSource) {
    return [];
  }
  return getSourceActorsForSource(state, selectedSource.id);
}

export function hasException(state, line, column) {
  return !!getSelectedException(state, line, column);
}

export function getSelectedException(state, line, column) {
  const sourceExceptions = getSelectedSourceExceptions(state);

  if (!sourceExceptions) {
    return;
  }

  return sourceExceptions.find(
    sourceExc =>
      sourceExc.lineNumber === line && sourceExc.columnNumber === column
  );
}

export default update;
