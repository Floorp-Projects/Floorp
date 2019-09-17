/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import typeof SourceMaps from "devtools-source-map";
import type { ThreadList, Thread, Context, ThreadId } from "../../types";
import type { State } from "../../reducers/types";
import type { MatchedLocations } from "../../reducers/file-search";
import type { TreeNode } from "../../utils/sources-tree/types";
import type { SearchOperation } from "../../reducers/project-text-search";

import type { BreakpointAction } from "./BreakpointAction";
import type { SourceAction } from "./SourceAction";
import type { SourceActorAction } from "./SourceActorAction";
import type { UIAction } from "./UIAction";
import type { PauseAction } from "./PauseAction";
import type { PreviewAction } from "./PreviewAction";
import type { ASTAction } from "./ASTAction";
import { clientCommands } from "../../client/firefox";
import type { Panel } from "../../client/firefox/types";
import type { ParserDispatcher } from "../../workers/parser";

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
  forkedDispatch: (action: any) => Promise<any>,
  getState: () => State,
  client: typeof clientCommands,
  sourceMaps: SourceMaps,
  parser: ParserDispatcher,
  evaluationsParser: ParserDispatcher,
  panel: Panel,
};

export type Thunk = ThunkArgs => any;

export type ActionType = Object | Function;

type ProjectTextSearchResult = {
  sourceId: string,
  filepath: string,
  matches: MatchedLocations[],
};

type AddTabAction = {|
  +type: "ADD_TAB",
  +url: string,
  +framework?: string,
  +isOriginal?: boolean,
  +sourceId?: string,
|};

type UpdateTabAction = {|
  +type: "UPDATE_TAB",
  +url: string,
  +framework?: string,
  +isOriginal?: boolean,
  +sourceId?: string,
|};

type NavigateAction =
  | {|
      +type: "CONNECT",
      +mainThread: Thread,
      +canRewind: boolean,
      +isWebExtension: boolean,
    |}
  | {| +type: "NAVIGATE", +mainThread: Thread |};

export type FocusItem = TreeNode;

export type SourceTreeAction =
  | {| +type: "SET_EXPANDED_STATE", +thread: string, +expanded: any |}
  | {| +type: "SET_FOCUSED_SOURCE_ITEM", +cx: Context, item: FocusItem |};

export type ProjectTextSearchAction =
  | {| +type: "ADD_QUERY", +cx: Context, +query: string |}
  | {|
      +type: "ADD_SEARCH_RESULT",
      +cx: Context,
      +result: ProjectTextSearchResult,
    |}
  | {| +type: "UPDATE_STATUS", +cx: Context, +status: string |}
  | {| +type: "CLEAR_SEARCH_RESULTS", +cx: Context |}
  | {|
      +type: "ADD_ONGOING_SEARCH",
      +cx: Context,
      +ongoingSearch: SearchOperation,
    |}
  | {| +type: "CLEAR_SEARCH", +cx: Context |};

export type FileTextSearchModifier =
  | "caseSensitive"
  | "wholeWord"
  | "regexMatch";

export type FileTextSearchAction =
  | {|
      +type: "TOGGLE_FILE_SEARCH_MODIFIER",
      +cx: Context,
      +modifier: FileTextSearchModifier,
    |}
  | {|
      +type: "UPDATE_FILE_SEARCH_QUERY",
      +cx: Context,
      +query: string,
    |}
  | {|
      +type: "UPDATE_SEARCH_RESULTS",
      +cx: Context,
      +results: {
        matches: MatchedLocations[],
        matchIndex: number,
        count: number,
        index: number,
      },
    |};

export type QuickOpenAction =
  | {| +type: "SET_QUICK_OPEN_QUERY", +query: string |}
  | {| +type: "OPEN_QUICK_OPEN", +query?: string |}
  | {| +type: "CLOSE_QUICK_OPEN" |};

export type DebuggeeAction =
  | {|
      +type: "INSERT_THREADS",
      +cx: Context,
      +threads: ThreadList,
    |}
  | {|
      +type: "REMOVE_THREADS",
      +cx: Context,
      +threads: Array<ThreadId>,
    |}
  | {|
      +type: "SELECT_THREAD",
      +cx: Context,
      +thread: ThreadId,
    |};

export type {
  StartPromiseAction,
  DonePromiseAction,
  ErrorPromiseAction,
} from "../utils/middleware/promise";

export type { panelPositionType } from "./UIAction";

export type { ASTAction } from "./ASTAction";

type ActiveEventListener = string;
type EventListenerEvent = { name: string, id: ActiveEventListener };
type EventListenerCategory = { name: string, events: EventListenerEvent[] };

export type EventListenerActiveList = ActiveEventListener[];
export type EventListenerCategoryList = EventListenerCategory[];
export type EventListenerExpandedList = string[];

export type EventListenerAction =
  | {|
      +type: "UPDATE_EVENT_LISTENERS",
      +active: EventListenerActiveList,
    |}
  | {|
      +type: "RECEIVE_EVENT_LISTENER_TYPES",
      +categories: EventListenerCategoryList,
    |}
  | {|
      +type: "UPDATE_EVENT_LISTENER_EXPANDED",
      +expanded: EventListenerExpandedList,
    |}
  | {|
      +type: "TOGGLE_EVENT_LISTENERS",
      +logEventBreakpoints: boolean,
    |};

/**
 * Actions: Source, Breakpoint, and Navigation
 *
 * @memberof actions/types
 * @static
 */
export type Action =
  | AddTabAction
  | UpdateTabAction
  | SourceActorAction
  | SourceAction
  | BreakpointAction
  | PauseAction
  | NavigateAction
  | UIAction
  | ASTAction
  | PreviewAction
  | QuickOpenAction
  | FileTextSearchAction
  | ProjectTextSearchAction
  | DebuggeeAction
  | SourceTreeAction
  | EventListenerAction;
