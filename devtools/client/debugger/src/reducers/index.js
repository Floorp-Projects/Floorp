/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Reducer index
 * @module reducers/index
 */

import expressions, { initialExpressionState } from "./expressions";
import sourceActors, { initialSourceActorsState } from "./source-actors";
import sources, { initialSourcesState } from "./sources";
import sourceBlackBox, { initialSourceBlackBoxState } from "./source-blackbox";
import sourcesContent, { initialSourcesContentState } from "./sources-content";
import tabs, { initialTabState } from "./tabs";
import breakpoints, { initialBreakpointsState } from "./breakpoints";
import pendingBreakpoints from "./pending-breakpoints";
import pause, { initialPauseState } from "./pause";
import ui, { initialUIState } from "./ui";
import ast, { initialASTState } from "./ast";
import quickOpen, { initialQuickOpenState } from "./quick-open";
import sourcesTree, { initialSourcesTreeState } from "./sources-tree";
import threads, { initialThreadsState } from "./threads";
import eventListenerBreakpoints, {
  initialEventListenerState,
} from "./event-listeners";
import exceptions, { initialExceptionsState } from "./exceptions";

import { objectInspector } from "devtools/client/shared/components/reps/index";

/**
 * Note that this is only used by jest tests.
 *
 * Production is using loadInitialState() in main.js
 */
export function initialState() {
  return {
    sources: initialSourcesState(),
    sourcesContent: initialSourcesContentState(),
    expressions: initialExpressionState(),
    sourceActors: initialSourceActorsState(),
    sourceBlackBox: initialSourceBlackBoxState(),
    tabs: initialTabState(),
    breakpoints: initialBreakpointsState(),
    pendingBreakpoints: {},
    pause: initialPauseState(),
    ui: initialUIState(),
    ast: initialASTState(),
    quickOpen: initialQuickOpenState(),
    sourcesTree: initialSourcesTreeState(),
    threads: initialThreadsState(),
    objectInspector: objectInspector.reducer.initialOIState(),
    eventListenerBreakpoints: initialEventListenerState(),
    exceptions: initialExceptionsState(),
  };
}

export default {
  expressions,
  sourceActors,
  sourceBlackBox,
  sourcesContent,
  sources,
  tabs,
  breakpoints,
  pendingBreakpoints,
  pause,
  ui,
  ast,
  quickOpen,
  sourcesTree,
  threads,
  objectInspector: objectInspector.reducer.default,
  eventListenerBreakpoints,
  exceptions,
};
