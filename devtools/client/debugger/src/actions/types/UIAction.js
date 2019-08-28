/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Source, Range, SourceLocation, Context } from "../../types";

import type {
  ActiveSearchType,
  OrientationType,
  SelectedPrimaryPaneTabType,
} from "../../reducers/ui";

export type panelPositionType = "start" | "end";

export type UIAction =
  | {|
      +type: "TOGGLE_ACTIVE_SEARCH",
      +value: ?ActiveSearchType,
    |}
  | {|
      +type: "OPEN_QUICK_OPEN",
      +query?: string,
    |}
  | {|
      +type: "CLOSE_QUICK_OPEN",
    |}
  | {|
      +type: "TOGGLE_FRAMEWORK_GROUPING",
      +value: boolean,
    |}
  | {|
      +type: "TOGGLE_INLINE_PREVIEW",
      +value: boolean,
    |}
  | {|
      +type: "SHOW_SOURCE",
      +source: Source,
    |}
  | {|
      +type: "TOGGLE_PANE",
      +position: panelPositionType,
      +paneCollapsed: boolean,
    |}
  | {|
      +type: "SET_ORIENTATION",
      +orientation: OrientationType,
    |}
  | {|
      +type: "HIGHLIGHT_LINES",
      +location: {
        start: number,
        end: number,
        sourceId: number,
      },
    |}
  | {|
      +type: "CLEAR_HIGHLIGHT_LINES",
    |}
  | {|
      +type: "OPEN_CONDITIONAL_PANEL",
      +location: SourceLocation,
      +log: boolean,
    |}
  | {|
      +type: "CLOSE_CONDITIONAL_PANEL",
    |}
  | {|
      +type: "SET_PROJECT_DIRECTORY_ROOT",
      +cx: Context,
      +url: string,
    |}
  | {|
      +type: "SET_PRIMARY_PANE_TAB",
      +tabName: SelectedPrimaryPaneTabType,
    |}
  | {|
      +type: "CLOSE_PROJECT_SEARCH",
    |}
  | {|
      +type: "SET_VIEWPORT",
      +viewport: Range,
    |};
