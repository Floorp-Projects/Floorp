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
import type { DebuggeeState } from "./debuggee";
import type { FileSearchState } from "./file-search";
import type { PauseState } from "./pause";
import type { PreviewState } from "./preview";
import type { PendingBreakpointsState } from "../selectors";
import type { ProjectTextSearchState } from "./project-text-search";
import type { Record } from "../utils/makeRecord";
import type { SourcesState } from "./sources";
import type { SourceActorsState } from "./source-actors";
import type { TabList } from "./tabs";
import type { UIState } from "./ui";
import type { QuickOpenState } from "./quick-open";

export type State = {
  ast: ASTState,
  breakpoints: BreakpointsState,
  expressions: Record<ExpressionState>,
  debuggee: DebuggeeState,
  fileSearch: Record<FileSearchState>,
  pause: PauseState,
  preview: PreviewState,
  pendingBreakpoints: PendingBreakpointsState,
  projectTextSearch: ProjectTextSearchState,
  sources: SourcesState,
  sourceActors: SourceActorsState,
  tabs: TabList,
  ui: Record<UIState>,
  quickOpen: Record<QuickOpenState>,
};

export type Selector<T> = State => T;

export type PendingSelectedLocation = {
  url: string,
  line?: number,
  column?: number,
};

export type {
  SourcesMap,
  SourcesMapByThread,
  SourceResourceState,
} from "./sources";
export type { ActiveSearchType, OrientationType } from "./ui";
export type { BreakpointsMap, XHRBreakpointsList } from "./breakpoints";
export type { Command } from "./pause";
export type { LoadedSymbols, Symbols } from "./ast";
export type { Preview } from "./preview";
