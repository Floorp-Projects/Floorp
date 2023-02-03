/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSourceActor,
  getSourceActorBreakableLines,
} from "../selectors/source-actors";
import { memoizeableAction } from "../utils/memoizableAction";
import { PROMISE } from "./utils/middleware/promise";

export function insertSourceActors(sourceActors) {
  return function({ dispatch }) {
    dispatch({
      type: "INSERT_SOURCE_ACTORS",
      sourceActors,
    });
  };
}

export const loadSourceActorBreakableLines = memoizeableAction(
  "loadSourceActorBreakableLines",
  {
    createKey: args => args.sourceActorId,
    getValue: ({ sourceActorId }, { getState }) =>
      getSourceActorBreakableLines(getState(), sourceActorId),
    action: async ({ sourceActorId }, { dispatch, getState, client }) => {
      await dispatch({
        type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
        sourceActorId,
        [PROMISE]: client.getSourceActorBreakableLines(
          getSourceActor(getState(), sourceActorId)
        ),
      });
    },
  }
);
