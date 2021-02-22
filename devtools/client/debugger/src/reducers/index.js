/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Reducer index
 * @module reducers/index
 */

import expressions, { initialExpressionState } from "./expressions";
import sourceActors from "./source-actors";
import sources, { initialSourcesState } from "./sources";
import tabs, { initialTabState } from "./tabs";
import breakpoints, { initialBreakpointsState } from "./breakpoints";
import pendingBreakpoints from "./pending-breakpoints";
import asyncRequests from "./async-requests";
import pause, { initialPauseState } from "./pause";
import ui, { initialUIState } from "./ui";
import fileSearch, { initialFileSearchState } from "./file-search";
import ast, { initialASTState } from "./ast";
import preview, { initialPreviewState } from "./preview";
import projectTextSearch, {
  initialProjectTextSearchState,
} from "./project-text-search";
import quickOpen, { initialQuickOpenState } from "./quick-open";
import sourceTree, { initialSourcesTreeState } from "./source-tree";
import threads, { initialThreadsState } from "./threads";
import eventListenerBreakpoints, {
  initialEventListenerState,
} from "./event-listeners";
import exceptions, { initialExceptionsState } from "./exceptions";

import { objectInspector } from "devtools/client/shared/components/reps/index";

import { createInitial } from "../utils/resource";

export function initialState() {
  return {
    sources: initialSourcesState(),
    expressions: initialExpressionState(),
    sourceActors: createInitial(),
    tabs: initialTabState(),
    breakpoints: initialBreakpointsState(),
    pendingBreakpoints: {},
    asyncRequests: [],
    pause: initialPauseState(),
    ui: initialUIState(),
    fileSearch: initialFileSearchState(),
    ast: initialASTState(),
    projectTextSearch: initialProjectTextSearchState(),
    quickOpen: initialQuickOpenState(),
    sourceTree: initialSourcesTreeState(),
    threads: initialThreadsState(),
    objectInspector: objectInspector.reducer.initialOIState(),
    eventListenerBreakpoints: initialEventListenerState(),
    preview: initialPreviewState(),
    exceptions: initialExceptionsState(),
  };
}

export default {
  expressions,
  sourceActors,
  sources,
  tabs,
  breakpoints,
  pendingBreakpoints,
  asyncRequests,
  pause,
  ui,
  fileSearch,
  ast,
  projectTextSearch,
  quickOpen,
  sourceTree,
  threads,
  objectInspector: objectInspector.reducer.default,
  eventListenerBreakpoints,
  preview,
  exceptions,
};
