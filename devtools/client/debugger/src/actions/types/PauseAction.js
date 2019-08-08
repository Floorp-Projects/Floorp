/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Command } from "../../reducers/types";
import type {
  Expression,
  LoadedObject,
  Frame,
  Scope,
  Why,
  ThreadContext,
} from "../../types";

import type { PromiseAction } from "../utils/middleware/promise";

export type PauseAction =
  | {|
      +type: "BREAK_ON_NEXT",
      +cx: ThreadContext,
      +thread: string,
      +value: boolean,
    |}
  | {|
      // Note: Do not include cx, as this action is triggered by the server.
      +type: "RESUME",
      +thread: string,
      +value: void,
      +wasStepping: boolean,
    |}
  | {|
      // Note: Do not include cx, as this action is triggered by the server.
      +type: "PAUSED",
      +thread: string,
      +why: Why,
      +scopes: Scope,
      +frames: Frame[],
      +selectedFrameId: string,
      +loadedObjects: LoadedObject[],
    |}
  | {|
      +type: "PAUSE_ON_EXCEPTIONS",
      +shouldPauseOnExceptions: boolean,
      +shouldPauseOnCaughtExceptions: boolean,
    |}
  | PromiseAction<{|
      +type: "COMMAND",
      +cx: ThreadContext,
      +thread: string,
      +command: Command,
    |}>
  | {|
      +type: "SELECT_FRAME",
      +cx: ThreadContext,
      +thread: string,
      +frame: Frame,
    |}
  | {|
      +type: "SELECT_COMPONENT",
      +thread: string,
      +componentIndex: number,
    |}
  | {|
      +type: "ADD_EXPRESSION",
      +cx: ThreadContext,
      +thread: string,
      +id: number,
      +input: string,
      +value: string,
      +expressionError: ?string,
    |}
  | PromiseAction<
      {|
        +type: "EVALUATE_EXPRESSION",
        +cx: ThreadContext,
        +thread: string,
        +input: string,
      |},
      Object
    >
  | PromiseAction<{|
      +type: "EVALUATE_EXPRESSIONS",
      +cx: ThreadContext,
      +results: Expression[],
      +inputs: string[],
    |}>
  | {|
      +type: "UPDATE_EXPRESSION",
      +cx: ThreadContext,
      +expression: Expression,
      +input: string,
      +expressionError: ?string,
    |}
  | {|
      +type: "DELETE_EXPRESSION",
      +input: string,
    |}
  | {|
      +type: "CLEAR_AUTOCOMPLETE",
    |}
  | {|
      +type: "CLEAR_EXPRESSION_ERROR",
    |}
  | {|
      +type: "AUTOCOMPLETE",
      +cx: ThreadContext,
      +input: string,
      +result: Object,
    |}
  | PromiseAction<
      {|
        +type: "MAP_SCOPES",
        +cx: ThreadContext,
        +thread: string,
        +frame: Frame,
      |},
      {
        scope: Scope,
        mappings: {
          [string]: string | null,
        },
      }
    >
  | {|
      +type: "MAP_FRAMES",
      +cx: ThreadContext,
      +thread: string,
      +frames: Frame[],
      +selectedFrameId: string,
    |}
  | PromiseAction<
      {|
        +type: "ADD_SCOPES",
        +cx: ThreadContext,
        +thread: string,
        +frame: Frame,
      |},
      Scope
    >
  | {|
      +type: "TOGGLE_SKIP_PAUSING",
      +thread: string,
      skipPausing: boolean,
    |}
  | {|
      +type: "TOGGLE_MAP_SCOPES",
      +mapScopes: boolean,
    |}
  | {|
      +type: "SET_EXPANDED_SCOPE",
      +cx: ThreadContext,
      +thread: string,
      +path: string,
      +expanded: boolean,
    |}
  | {|
      +type: "ADD_INLINE_PREVIEW",
      +thread: string,
      +frame: Frame,
      +previewData: any,
    |};
