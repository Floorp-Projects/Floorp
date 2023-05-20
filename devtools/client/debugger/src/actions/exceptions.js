/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function addExceptionFromResources(resources) {
  return async function ({ dispatch }) {
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
        threadActorId: resource.targetFront.targetForm.threadActor,
      };

      dispatch({
        type: "ADD_EXCEPTION",
        exception,
      });
    }
  };
}
