/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Exceptions reducer
 * @module reducers/exceptionss
 */

import { createSelector } from "reselect";
import { getSelectedSource, getSourceActorsForSource } from "../selectors";

import type { Exception, SourceActorId, SourceActor } from "../types";

import type { Action } from "../actions/types";
import type { Selector, State } from "./types";

export type ExceptionMap = { [SourceActorId]: Exception[] };

export type ExceptionState = {
  exceptions: ExceptionMap,
};

export function initialExceptionsState() {
  return {
    exceptions: {},
  };
}

function update(
  state: ExceptionState = initialExceptionsState(),
  action: Action
): ExceptionState {
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
export function getExceptionsMap(state: State): ExceptionMap {
  return state.exceptions.exceptions;
}

export const getSelectedSourceExceptions: Selector<
  Exception[]
> = createSelector(
  getSelectedSourceActors,
  getExceptionsMap,
  (sourceActors: Array<SourceActor>, exceptions) => {
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

export function hasException(
  state: State,
  line: number,
  column: number
): boolean {
  return !!getSelectedException(state, line, column);
}

export function getSelectedException(
  state: State,
  line: number,
  column: number
): ?Exception {
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
