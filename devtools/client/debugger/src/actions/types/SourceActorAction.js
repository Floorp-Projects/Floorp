/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { type PromiseAction } from "../utils/middleware/promise";
import type {
  SourceActorId,
  SourceActor,
} from "../../reducers/source-actors.js";

export type SourceActorsInsertAction = {|
  type: "INSERT_SOURCE_ACTORS",
  items: Array<SourceActor>,
|};
export type SourceActorsRemoveAction = {|
  type: "REMOVE_SOURCE_ACTORS",
  items: Array<SourceActor>,
|};

export type SourceActorBreakpointColumnsAction = PromiseAction<
  {|
    type: "SET_SOURCE_ACTOR_BREAKPOINT_COLUMNS",
    sourceId: SourceActorId,
    line: number,
  |},
  Array<number>
>;

export type SourceActorBreakableLinesAction = PromiseAction<
  {|
    type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
    sourceId: SourceActorId,
  |},
  Array<number>
>;

export type SourceActorAction =
  | SourceActorsInsertAction
  | SourceActorsRemoveAction
  | SourceActorBreakpointColumnsAction
  | SourceActorBreakableLinesAction;
