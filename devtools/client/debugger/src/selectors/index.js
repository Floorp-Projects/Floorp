/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export * from "../reducers/expressions";
export * from "../reducers/sources";
export * from "../reducers/tabs";
export * from "../reducers/pause";
export * from "../reducers/threads";
export * from "../reducers/breakpoints";
export * from "../reducers/pending-breakpoints";
export * from "../reducers/ui";

export {
  getSourceActor,
  hasSourceActor,
  getSourceActors,
  getSourceActorsForThread,
} from "../reducers/source-actors";

export * from "./ast";
export { getXHRBreakpoints, shouldPauseOnAnyXHR } from "./breakpoints";
export {
  getClosestBreakpoint,
  getBreakpointAtLocation,
  getBreakpointsAtLine,
  getClosestBreakpointPosition,
} from "./breakpointAtLocation";
export { getBreakpointSources } from "./breakpointSources";
export * from "./event-listeners";
export * from "./exceptions";
export * from "./file-search";
export { getCallStackFrames } from "./getCallStackFrames";
export { isLineInScope } from "./isLineInScope";
export { isSelectedFrameVisible } from "./isSelectedFrameVisible";
export {
  getSelectedFrame,
  getSelectedFrames,
  getVisibleSelectedFrame,
} from "./pause";
export * from "./preview";
export * from "./project-text-search";
export * from "./quick-open";
export * from "./source-tree";
export * from "./tabs";
export * from "./threads";
export {
  getVisibleBreakpoints,
  getFirstVisibleBreakpoints,
} from "./visibleBreakpoints";
export * from "./visibleColumnBreakpoints";

import { objectInspector } from "devtools/client/shared/components/reps/index";

const { reducer } = objectInspector;

Object.keys(reducer).forEach(function(key) {
  if (key === "default" || key === "__esModule") {
    return;
  }
  Object.defineProperty(exports, key, {
    enumerable: true,
    get: reducer[key],
  });
});
