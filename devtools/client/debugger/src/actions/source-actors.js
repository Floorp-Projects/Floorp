/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSourceActor,
  getSourceActorBreakableLines,
  getSourceActorBreakpointColumns,
} from "../reducers/source-actors";
import { memoizeableAction } from "../utils/memoizableAction";
import { PROMISE } from "./utils/middleware/promise";

export function insertSourceActors(items) {
  return function({ dispatch }) {
    dispatch({
      type: "INSERT_SOURCE_ACTORS",
      items,
    });
  };
}

export function removeSourceActor(item) {
  return removeSourceActors([item]);
}
export function removeSourceActors(items) {
  return function({ dispatch }) {
    dispatch({ type: "REMOVE_SOURCE_ACTORS", items });
  };
}

export const loadSourceActorBreakpointColumns = memoizeableAction(
  "loadSourceActorBreakpointColumns",
  {
    createKey: ({ id, line }) => `${id}:${line}`,
    getValue: ({ id, line }, { getState }) =>
      getSourceActorBreakpointColumns(getState(), id, line),
    action: async ({ id, line }, { dispatch, getState, client }) => {
      await dispatch({
        type: "SET_SOURCE_ACTOR_BREAKPOINT_COLUMNS",
        sourceId: id,
        line,
        [PROMISE]: (async () => {
          const positions = await client.getSourceActorBreakpointPositions(
            getSourceActor(getState(), id),
            {
              start: { line, column: 0 },
              end: { line: line + 1, column: 0 },
            }
          );

          return positions[line] || [];
        })(),
      });
    },
  }
);

export const loadSourceActorBreakableLines = memoizeableAction(
  "loadSourceActorBreakableLines",
  {
    createKey: args => args.id,
    getValue: ({ id }, { getState }) =>
      getSourceActorBreakableLines(getState(), id),
    action: async ({ id }, { dispatch, getState, client }) => {
      await dispatch({
        type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
        sourceId: id,
        [PROMISE]: client.getSourceActorBreakableLines(
          getSourceActor(getState(), id)
        ),
      });
    },
  }
);
