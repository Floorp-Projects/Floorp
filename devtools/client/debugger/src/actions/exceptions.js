/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { hasException } from "../selectors";

import type { ThunkArgs } from "./types";
import type { Exception } from "../types";

export function addExceptionFromResource({ resource }: Object) {
  return async function({ dispatch }: ThunkArgs) {
    const { pageError } = resource;
    if (!pageError.error) {
      return;
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
  };
}

export function addException(exception: Exception) {
  return async function({ dispatch, getState }: ThunkArgs) {
    const { columnNumber, lineNumber } = exception;

    if (!hasException(getState(), lineNumber, columnNumber)) {
      dispatch({
        type: "ADD_EXCEPTION",
        exception,
      });
    }
  };
}
