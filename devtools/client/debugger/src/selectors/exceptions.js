/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "devtools/client/shared/vendor/reselect";
import { shallowEqual, arrayShallowEqual } from "../utils/shallow-equal";

import { getSelectedSource, getSourceActorsForSource } from "./index";

export const getSelectedSourceExceptions = createSelector(
  getSelectedSourceActors,
  // Do not retrieve mutableExceptionsMap as it will never change and createSelector would
  // prevent re-running the selector in case of modification. state.exception is the `state`
  // in the reducer, which we take care of cloning in case of new exception.
  state => state.exceptions,
  (sourceActors, exceptionsState) => {
    const { mutableExceptionsMap } = exceptionsState;
    const sourceExceptions = [];

    for (const sourceActor of sourceActors) {
      const exceptions = mutableExceptionsMap.get(sourceActor.id);
      if (exceptions) {
        sourceExceptions.push(...exceptions);
      }
    }

    return sourceExceptions;
  },
  // Shallow compare both input and output because of arrays being possibly always
  // different instance but with same content.
  {
    memoizeOptions: {
      equalityCheck: shallowEqual,
      resultEqualityCheck: arrayShallowEqual,
    },
  }
);

function getSelectedSourceActors(state) {
  const selectedSource = getSelectedSource(state);
  if (!selectedSource) {
    return [];
  }
  return getSourceActorsForSource(state, selectedSource.id);
}

export function getSelectedException(state, line, column) {
  const sourceExceptions = getSelectedSourceExceptions(state);

  if (!sourceExceptions) {
    return undefined;
  }

  return sourceExceptions.find(
    sourceExc =>
      sourceExc.lineNumber === line && sourceExc.columnNumber === column
  );
}
