/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SourceActor } from "../../reducers/source-actors.js";

export type SourceActorsInsertAction = {|
  type: "INSERT_SOURCE_ACTORS",
  items: Array<SourceActor>,
|};
export type SourceActorsRemoveAction = {|
  type: "REMOVE_SOURCE_ACTORS",
  items: Array<SourceActor>,
|};

export type SourceActorAction =
  | SourceActorsInsertAction
  | SourceActorsRemoveAction;
