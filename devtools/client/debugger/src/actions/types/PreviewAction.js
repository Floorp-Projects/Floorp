/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Context } from "../../types";
import type { Preview } from "../../reducers/types";

export type PreviewAction =
  | {|
      +type: "SET_PREVIEW",
      +cx: Context,
      value: Preview,
    |}
  | {|
      +type: "CLEAR_PREVIEW",
      +cx: Context,
    |};
