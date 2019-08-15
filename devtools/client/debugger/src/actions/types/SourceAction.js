/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SourceId, Source, SourceLocation, Context } from "../../types";
import type { PromiseAction } from "../utils/middleware/promise";
import type { SourceBase } from "../../reducers/sources";

export type LoadSourceAction = PromiseAction<
  {|
    +type: "LOAD_SOURCE_TEXT",
    +cx: Context,
    +sourceId: string,
    +epoch: number,
  |},
  {
    text: string | {| binary: Object |},
    contentType: string | void,
  }
>;
export type SourceAction =
  | LoadSourceAction
  | {|
      +type: "ADD_SOURCE",
      +cx: Context,
      +source: SourceBase,
    |}
  | {|
      +type: "ADD_SOURCES",
      +cx: Context,
      +sources: Array<SourceBase>,
    |}
  | {|
      +type: "CLEAR_SOURCE_MAP_URL",
      +cx: Context,
      +sourceId: SourceId,
    |}
  | {|
      +type: "SET_SELECTED_LOCATION",
      +cx: Context,
      +source: Source,
      +location?: SourceLocation,
    |}
  | {|
      +type: "SET_PENDING_SELECTED_LOCATION",
      +cx: Context,
      +url: string,
      +line?: number,
      +column?: number,
    |}
  | {| type: "CLEAR_SELECTED_LOCATION", +cx: Context |}
  | PromiseAction<
      {|
        +type: "BLACKBOX",
        +cx: Context,
        +source: Source,
      |},
      {|
        +isBlackBoxed: boolean,
      |}
    >
  | {|
      +type: "MOVE_TAB",
      +url: string,
      +tabIndex: number,
    |}
  | {|
      +type: "CLOSE_TAB",
      +url: string,
      +tabs: any,
    |}
  | {|
      +type: "CLOSE_TABS",
      +sources: Array<Source>,
      +tabs: any,
    |}
  | {|
      type: "SET_ORIGINAL_BREAKABLE_LINES",
      +cx: Context,
      breakableLines: number[],
      sourceId: string,
    |};
