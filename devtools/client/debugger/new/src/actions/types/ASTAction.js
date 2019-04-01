/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SymbolDeclarations, AstLocation } from "../../workers/parser";
import type { PromiseAction } from "../utils/middleware/promise";

export type ASTAction =
  | PromiseAction<
      {|
        +type: "SET_SYMBOLS",
        +sourceId: string
      |},
      SymbolDeclarations
    >
  | {|
      +type: "OUT_OF_SCOPE_LOCATIONS",
      +locations: ?(AstLocation[])
    |}
  | {|
      +type: "IN_SCOPE_LINES",
      +lines: number[]
    |}
  | PromiseAction<
      {|
        +type: "SET_PREVIEW"
      |},
      {
        expression: string,
        result: any,
        location: AstLocation,
        tokenPos: any,
        cursorPos: any
      }
    >
  | {|
      +type: "CLEAR_SELECTION"
    |};
