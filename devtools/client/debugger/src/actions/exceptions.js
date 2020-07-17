/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { hasException } from "../selectors";

import type { ThunkArgs } from "./types";

export function addException({ resource }: Object) {
  return async function({ dispatch, getState }: ThunkArgs) {
    const { pageError } = resource;
    if (!pageError.error) {
      return;
    }
    const { columnNumber, lineNumber, sourceId, errorMessage } = pageError;
    const stacktrace = pageError.stacktrace || [];

    if (!hasException(getState(), lineNumber, columnNumber)) {
      dispatch({
        type: "ADD_EXCEPTION",
        exception: {
          columnNumber,
          lineNumber,
          sourceActorId: sourceId,
          errorMessage,
          stacktrace,
        },
      });
    }
  };
}
