/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { hasException } from "../selectors";

export function addExceptionFromResources(resources) {
  return async function({ dispatch }) {
    for (const resource of resources) {
      const { pageError } = resource;
      if (!pageError.error) {
        continue;
      }
      const { columnNumber, lineNumber, sourceId, errorMessage } = pageError;
      const stacktrace = pageError.stacktrace || [];

      const exception = {
        columnNumber,
        lineNumber,
        sourceActorId: sourceId,
        errorMessage,
        stacktrace,
      };

      dispatch(addException(exception));
    }
  };
}

export function addException(exception) {
  return async function({ dispatch, getState }) {
    const { columnNumber, lineNumber } = exception;

    if (!hasException(getState(), lineNumber, columnNumber)) {
      dispatch({
        type: "ADD_EXCEPTION",
        exception,
      });
    }
  };
}
