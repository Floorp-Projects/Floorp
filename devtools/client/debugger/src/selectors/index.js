/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export * from "../reducers/expressions";
export * from "../reducers/sources";
export * from "../reducers/tabs";
export * from "../reducers/event-listeners";
export * from "../reducers/pause";
export * from "../reducers/debuggee";
export * from "../reducers/breakpoints";
export * from "../reducers/pending-breakpoints";
export * from "../reducers/ui";
export * from "../reducers/file-search";
export * from "../reducers/ast";
export * from "../reducers/project-text-search";
export * from "../reducers/source-tree";
export * from "../reducers/preview";

export {
  getSourceActor,
  hasSourceActor,
  getSourceActors,
  getSourceActorsForThread,
} from "../reducers/source-actors";

export {
  getQuickOpenEnabled,
  getQuickOpenQuery,
  getQuickOpenType,
} from "../reducers/quick-open";

export {
  getBreakpointAtLocation,
  getBreakpointsAtLine,
} from "./breakpointAtLocation";
export {
  getVisibleBreakpoints,
  getFirstVisibleBreakpoints,
} from "./visibleBreakpoints";
export { inComponent } from "./inComponent";
export { isSelectedFrameVisible } from "./isSelectedFrameVisible";
export { getCallStackFrames } from "./getCallStackFrames";
export { getBreakpointSources } from "./breakpointSources";
export { getXHRBreakpoints, shouldPauseOnAnyXHR } from "./breakpoints";
export * from "./visibleColumnBreakpoints";
export {
  getSelectedFrame,
  getSelectedFrames,
  getVisibleSelectedFrame,
} from "./pause";

// eslint-disable-next-line import/named
import { objectInspector } from "devtools-reps";

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
