/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Source, SourceLocation, Context } from "../../types";
import type { PromiseAction } from "../utils/middleware/promise";

export type LoadSourceAction = PromiseAction<
  {|
    +type: "LOAD_SOURCE_TEXT",
    +cx: Context,
    +sourceId: string,
    +epoch: number
  |},
  Source
>;
export type SourceAction =
  | LoadSourceAction
  | {|
      +type: "ADD_SOURCE",
      +cx: Context,
      +source: Source
    |}
  | {|
      +type: "ADD_SOURCES",
      +cx: Context,
      +sources: Array<Source>
    |}
  | {|
      +type: "UPDATE_SOURCE",
      +cx: Context,
      +source: Source
    |}
  | {|
      +type: "SET_SELECTED_LOCATION",
      +cx: Context,
      +source: Source,
      +location?: SourceLocation
    |}
  | {|
      +type: "SET_PENDING_SELECTED_LOCATION",
      +cx: Context,
      +url: string,
      +line?: number,
      +column?: number
    |}
  | {| type: "CLEAR_SELECTED_LOCATION", +cx: Context |}
  | PromiseAction<
      {|
        +type: "BLACKBOX",
        +cx: Context,
        +source: Source
      |},
      {|
        +isBlackBoxed: boolean
      |}
    >
  | {|
      +type: "MOVE_TAB",
      +url: string,
      +tabIndex: number
    |}
  | {|
      +type: "CLOSE_TAB",
      +url: string,
      +tabs: any
    |}
  | {|
      +type: "CLOSE_TABS",
      +sources: Array<Source>,
      +tabs: any
    |};
