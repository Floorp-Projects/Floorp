/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "./types";
import {
  getSourceActor,
  getSourceActorBreakableLines,
  getSourceActorBreakpointColumns,
  type SourceActorId,
  type SourceActor,
} from "../reducers/source-actors";
import {
  memoizeableAction,
  type MemoizedAction,
} from "../utils/memoizableAction";
import { PROMISE } from "./utils/middleware/promise";

export function insertSourceActor(item: SourceActor) {
  return insertSourceActors([item]);
}
export function insertSourceActors(items: Array<SourceActor>) {
  return function({ dispatch }: ThunkArgs) {
    dispatch({
      type: "INSERT_SOURCE_ACTORS",
      items,
    });
  };
}

export function removeSourceActor(item: SourceActor) {
  return removeSourceActors([item]);
}
export function removeSourceActors(items: Array<SourceActor>) {
  return function({ dispatch }: ThunkArgs) {
    dispatch({
      type: "REMOVE_SOURCE_ACTORS",
      items,
    });
  };
}

export const loadSourceActorBreakpointColumns: MemoizedAction<
  { id: SourceActorId, line: number },
  Array<number>
> = memoizeableAction("loadSourceActorBreakpointColumns", {
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
});

export const loadSourceActorBreakableLines: MemoizedAction<
  { id: SourceActorId },
  Array<number>
> = memoizeableAction("loadSourceActorBreakableLines", {
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
});
