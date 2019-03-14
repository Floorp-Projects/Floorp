/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type {
  Breakpoint,
  SourceLocation,
  XHRBreakpoint,
  Source,
  BreakpointPositions
} from "../../types";

import type { PromiseAction } from "../utils/middleware/promise";

export type BreakpointAction =
  | PromiseAction<
      {|
        +type: "ADD_BREAKPOINT",
        +breakpoint: Breakpoint,
        +condition?: string
      |},
      Breakpoint
    >
  | PromiseAction<{|
      +type: "REMOVE_BREAKPOINT",
      +breakpoint: Breakpoint,
      +disabled: boolean
    |}>
  | PromiseAction<{|
      +type: "SET_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint
    |}>
  | PromiseAction<{|
      +type: "ENABLE_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint,
      +index: number
    |}>
  | PromiseAction<{|
      +type: "UPDATE_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint,
      +index: number
    |}>
  | PromiseAction<{|
      +type: "DISABLE_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint,
      +index: number
    |}>
  | PromiseAction<{|
      +type: "REMOVE_XHR_BREAKPOINT",
      +index: number,
      +breakpoint: XHRBreakpoint
    |}>
  | {|
      +type: "REMOVE_BREAKPOINT",
      +breakpoint: Breakpoint,
      +status: "done"
    |}
  | {|
      +type: "SET_BREAKPOINT_OPTIONS",
      +breakpoint: Breakpoint
    |}
  | PromiseAction<{|
      +type: "TOGGLE_BREAKPOINTS",
      +shouldDisableBreakpoints: boolean
    |}>
  | {|
      +type: "SYNC_BREAKPOINT",
      +breakpoint: ?Breakpoint,
      +previousLocation: SourceLocation
    |}
  | PromiseAction<
      {|
        +type: "ENABLE_BREAKPOINT",
        +breakpoint: Breakpoint
      |},
      Breakpoint
    >
  | {|
      +type: "DISABLE_BREAKPOINT",
      +breakpoint: Breakpoint
    |}
  | {|
      +type: "DISABLE_ALL_BREAKPOINTS",
      +breakpoints: Breakpoint[]
    |}
  | {|
      +type: "ENABLE_ALL_BREAKPOINTS",
      +breakpoints: Breakpoint[]
    |}
  | {|
      +type: "REMAP_BREAKPOINTS",
      +breakpoints: Breakpoint[]
    |}
  | {|
      type: "ADD_BREAKPOINT_POSITIONS",
      positions: BreakpointPositions,
      source: Source
    |};
