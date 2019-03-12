/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Command } from "../../reducers/types";
import type { Expression, LoadedObject, Frame, Scope, Why } from "../../types";

import type { PromiseAction } from "../utils/middleware/promise";

export type PauseAction =
  | {|
      +type: "BREAK_ON_NEXT",
      +thread: string,
      +value: boolean
    |}
  | {|
      +type: "RESUME",
      +thread: string,
      +value: void,
      +wasStepping: boolean
    |}
  | {|
      +type: "PAUSED",
      +thread: string,
      +why: Why,
      +scopes: Scope,
      +frames: Frame[],
      +selectedFrameId: string,
      +loadedObjects: LoadedObject[]
    |}
  | {|
      +type: "PAUSE_ON_EXCEPTIONS",
      +shouldPauseOnExceptions: boolean,
      +shouldPauseOnCaughtExceptions: boolean
    |}
  | PromiseAction<{|
      +type: "COMMAND",
      +thread: string,
      +command: Command
    |}>
  | {|
      +type: "SELECT_FRAME",
      +thread: string,
      +frame: Frame
    |}
  | {|
      +type: "SELECT_COMPONENT",
      +thread: string,
      +componentIndex: number
    |}
  | {|
      +type: "SET_POPUP_OBJECT_PROPERTIES",
      +thread: string,
      +objectId: string,
      +properties: Object
    |}
  | {|
      +type: "ADD_EXPRESSION",
      +thread: string,
      +id: number,
      +input: string,
      +value: string,
      +expressionError: ?string
    |}
  | PromiseAction<
      {|
        +type: "EVALUATE_EXPRESSION",
        +thread: string,
        +input: string
      |},
      Object
    >
  | PromiseAction<{|
      +type: "EVALUATE_EXPRESSIONS",
      +results: Expression[],
      +inputs: string[]
    |}>
  | {|
      +type: "UPDATE_EXPRESSION",
      +expression: Expression,
      +input: string,
      +expressionError: ?string
    |}
  | {|
      +type: "DELETE_EXPRESSION",
      +input: string
    |}
  | {|
      +type: "CLEAR_AUTOCOMPLETE"
    |}
  | {|
      +type: "CLEAR_EXPRESSION_ERROR"
    |}
  | {|
      +type: "AUTOCOMPLETE",
      +input: string,
      +result: Object
    |}
  | PromiseAction<
      {|
        +type: "MAP_SCOPES",
        +thread: string,
        +frame: Frame
      |},
      {
        scope: Scope,
        mappings: {
          [string]: string | null
        }
      }
    >
  | {|
      +type: "MAP_FRAMES",
      +thread: string,
      +frames: Frame[],
      +selectedFrameId: string
    |}
  | PromiseAction<
      {|
        +type: "ADD_SCOPES",
        +thread: string,
        +frame: Frame
      |},
      Scope
    >
  | {|
      +type: "TOGGLE_SKIP_PAUSING",
      +thread: string,
      skipPausing: boolean
    |}
  | {|
      +type: "TOGGLE_MAP_SCOPES",
      +mapScopes: boolean
    |};
