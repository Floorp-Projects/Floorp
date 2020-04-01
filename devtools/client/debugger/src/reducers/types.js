/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Types reducer
 * @module reducers/types
 */

// @flow

import type { ASTState } from "./ast";
import type { BreakpointsState } from "./breakpoints";
import type { ExpressionState } from "./expressions";
import type { ThreadsState } from "./threads";
import type { FileSearchState } from "./file-search";
import type { PauseState } from "./pause";
import type { PreviewState } from "./preview";
import type { PendingBreakpointsState } from "../selectors";
import type { ProjectTextSearchState } from "./project-text-search";
import type { SourcesState } from "./sources";
import type { SourceActorsState } from "./source-actors";
import type { TabsState } from "./tabs";
import type { UIState } from "./ui";
import type { QuickOpenState } from "./quick-open";
import type { SourceTreeState } from "./source-tree";
import type { EventListenersState } from "./event-listeners";
import type { URL } from "../types";

export type State = {
  ast: ASTState,
  breakpoints: BreakpointsState,
  expressions: ExpressionState,
  eventListenerBreakpoints: EventListenersState,
  threads: ThreadsState,
  fileSearch: FileSearchState,
  pause: PauseState,
  preview: PreviewState,
  pendingBreakpoints: PendingBreakpointsState,
  projectTextSearch: ProjectTextSearchState,
  sources: SourcesState,
  sourceActors: SourceActorsState,
  sourceTree: SourceTreeState,
  tabs: TabsState,
  ui: UIState,
  quickOpen: QuickOpenState,
};

export type Selector<T> = State => T;

export type PendingSelectedLocation = {
  url: URL,
  line?: number,
  column?: number,
};

export type {
  SourcesMap,
  SourcesMapByThread,
  SourceBase,
  SourceResourceState,
  SourceResource,
} from "./sources";
export type { ActiveSearchType, OrientationType } from "./ui";
export type { BreakpointsMap, XHRBreakpointsList } from "./breakpoints";
export type { Command } from "./pause";
export type { LoadedSymbols, Symbols } from "./ast";
export type { Preview } from "./preview";
export type { Tab, TabList, TabsSources } from "./tabs";
