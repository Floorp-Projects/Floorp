/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Source, SourceLocation } from "../../types";
import type { PromiseAction } from "../utils/middleware/promise";

export type LoadSourceAction = PromiseAction<
  {|
    +type: "LOAD_SOURCE_TEXT",
    +sourceId: string,
    +epoch: number
  |},
  Source
>;
export type SourceAction =
  | LoadSourceAction
  | {|
      +type: "ADD_SOURCE",
      +source: Source
    |}
  | {|
      +type: "ADD_SOURCES",
      +sources: Array<Source>
    |}
  | {|
      +type: "UPDATE_SOURCE",
      +source: Source
    |}
  | {|
      +type: "SET_SELECTED_LOCATION",
      +source: Source,
      +location?: SourceLocation
    |}
  | {|
      +type: "SET_PENDING_SELECTED_LOCATION",
      +url: string,
      +line?: number,
      +column?: number
    |}
  | {| type: "CLEAR_SELECTED_LOCATION" |}
  | PromiseAction<
      {|
        +type: "BLACKBOX",
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
