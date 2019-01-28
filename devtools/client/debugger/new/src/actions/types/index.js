/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type {
  Frame,
  Scope,
  Why,
  Worker,
  WorkerList,
  MainThread
} from "../../types";
import type { State } from "../../reducers/types";
import type { MatchedLocations } from "../../reducers/file-search";
import type { TreeNode } from "../../utils/sources-tree/types";
import type { SearchOperation } from "../../reducers/project-text-search";

import type { BreakpointAction } from "./BreakpointAction";
import type { SourceAction } from "./SourceAction";
import type { UIAction } from "./UIAction";
import type { PauseAction } from "./PauseAction";
import type { ASTAction } from "./ASTAction";

/**
 * Flow types
 * @module actions/types
 */

/**
 * Argument parameters via Thunk middleware for {@link https://github.com/gaearon/redux-thunk|Redux Thunk}
 *
 * @memberof actions/breakpoints
 * @static
 * @typedef {Object} ThunkArgs
 */
export type ThunkArgs = {
  dispatch: (action: any) => Promise<any>,
  getState: () => State,
  client: any,
  sourceMaps: any,
  openLink: (url: string) => void,
  openWorkerToolbox: (worker: Worker) => void,
  openElementInInspector: (grip: Object) => void,
  onReload: () => void
};

export type Thunk = ThunkArgs => any;

export type ActionType = Object | Function;

type ProjectTextSearchResult = {
  sourceId: string,
  filepath: string,
  matches: MatchedLocations[]
};

type AddTabAction = {|
  +type: "ADD_TAB",
  +url: string,
  +framework?: string,
  +isOriginal?: boolean,
  +sourceId?: string,
  +thread: string
|};

type UpdateTabAction = {|
  +type: "UPDATE_TAB",
  +url: string,
  +framework?: string,
  +isOriginal?: boolean,
  +sourceId?: string,
  +thread: string
|};

type ReplayAction =
  | {|
      +type: "TRAVEL_TO",
      +data: {
        paused: {
          why: Why,
          scopes: Scope[],
          frames: Frame[],
          selectedFrameId: string,
          loadedObjects: Object
        },
        expressions?: Object[]
      },
      +position: number
    |}
  | {|
      +type: "CLEAR_HISTORY"
    |};

type NavigateAction =
  | {| +type: "CONNECT", +mainThread: MainThread, +canRewind: boolean |}
  | {| +type: "NAVIGATE", +mainThread: MainThread |};

export type FocusItem = {
  thread: string,
  item: TreeNode
};

export type SourceTreeAction =
  | {| +type: "SET_EXPANDED_STATE", +thread: string, +expanded: any |}
  | {| +type: "SET_FOCUSED_SOURCE_ITEM", item: FocusItem |};

export type ProjectTextSearchAction =
  | {| +type: "ADD_QUERY", +query: string |}
  | {|
      +type: "ADD_SEARCH_RESULT",
      +result: ProjectTextSearchResult
    |}
  | {| +type: "UPDATE_STATUS", +status: string |}
  | {| +type: "CLEAR_SEARCH_RESULTS" |}
  | {| +type: "ADD_ONGOING_SEARCH", +ongoingSearch: SearchOperation |}
  | {| +type: "CLEAR_SEARCH" |};

export type FileTextSearchModifier =
  | "caseSensitive"
  | "wholeWord"
  | "regexMatch";

export type FileTextSearchAction =
  | {|
      +type: "TOGGLE_FILE_SEARCH_MODIFIER",
      +modifier: FileTextSearchModifier
    |}
  | {|
      +type: "UPDATE_FILE_SEARCH_QUERY",
      +query: string
    |}
  | {|
      +type: "UPDATE_SEARCH_RESULTS",
      +results: {
        matches: MatchedLocations[],
        matchIndex: number,
        count: number,
        index: number
      }
    |};

export type QuickOpenAction =
  | {| +type: "SET_QUICK_OPEN_QUERY", +query: string |}
  | {| +type: "OPEN_QUICK_OPEN", +query?: string |}
  | {| +type: "CLOSE_QUICK_OPEN" |};

export type DebugeeAction =
  | {|
      +type: "SET_WORKERS",
      +workers: WorkerList
    |}
  | {|
      +type: "SELECT_THREAD",
      +thread: string
    |};

export type {
  StartPromiseAction,
  DonePromiseAction,
  ErrorPromiseAction
} from "../utils/middleware/promise";

export type { panelPositionType } from "./UIAction";

export type { ASTAction } from "./ASTAction";

/**
 * Actions: Source, Breakpoint, and Navigation
 *
 * @memberof actions/types
 * @static
 */
export type Action =
  | AddTabAction
  | UpdateTabAction
  | SourceAction
  | BreakpointAction
  | PauseAction
  | NavigateAction
  | UIAction
  | ASTAction
  | QuickOpenAction
  | FileTextSearchAction
  | ProjectTextSearchAction
  | DebugeeAction
  | ReplayAction
  | SourceTreeAction;
