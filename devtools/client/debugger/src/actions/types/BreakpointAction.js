/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type {
  Breakpoint,
  SourceLocation,
  XHRBreakpoint,
  Source,
  BreakpointPositions,
  PendingLocation,
  Context,
} from "../../types";

import type { PromiseAction } from "../utils/middleware/promise";

export type BreakpointAction =
  | PromiseAction<{|
      +type: "SET_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint,
    |}>
  | PromiseAction<{|
      +type: "ENABLE_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint,
      +index: number,
    |}>
  | PromiseAction<{|
      +type: "UPDATE_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint,
      +index: number,
    |}>
  | PromiseAction<{|
      +type: "DISABLE_XHR_BREAKPOINT",
      +breakpoint: XHRBreakpoint,
      +index: number,
    |}>
  | PromiseAction<{|
      +type: "REMOVE_XHR_BREAKPOINT",
      +index: number,
      +breakpoint: XHRBreakpoint,
    |}>
  | {|
      +type: "SET_BREAKPOINT",
      +cx: Context,
      +breakpoint: Breakpoint,
    |}
  | {|
      +type: "REMOVE_BREAKPOINT",
      +cx: Context,
      +location: SourceLocation,
    |}
  | {|
      +type: "REMOVE_PENDING_BREAKPOINT",
      +cx: Context,
      +location: PendingLocation,
    |}
  | {|
      type: "ADD_BREAKPOINT_POSITIONS",
      +cx: Context,
      positions: BreakpointPositions,
      source: Source,
    |};
