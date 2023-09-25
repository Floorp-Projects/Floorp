/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export * from "./ast";
export * from "./breakpoints";
export {
  getClosestBreakpoint,
  getBreakpointAtLocation,
  getBreakpointsAtLine,
  getClosestBreakpointPosition,
} from "./breakpointAtLocation";
export { getBreakpointSources } from "./breakpointSources";
export * from "./event-listeners";
export * from "./exceptions";
export * from "./expressions";
export { isLineInScope } from "./isLineInScope";
export { isSelectedFrameVisible } from "./isSelectedFrameVisible";
export * from "./pause";
export * from "./pending-breakpoints";
export * from "./quick-open";
export * from "./source-actors";
export * from "./source-blackbox";
export * from "./sources-content";
export * from "./sources-tree";
export * from "./sources";
export * from "./tabs";
export * from "./threads";
export * from "./ui";
export {
  getVisibleBreakpoints,
  getFirstVisibleBreakpoints,
} from "./visibleBreakpoints";
export * from "./visibleColumnBreakpoints";

import { objectInspector } from "devtools/client/shared/components/reps/index";

const { reducer } = objectInspector;

Object.keys(reducer).forEach(function (key) {
  if (key === "default" || key === "__esModule") {
    return;
  }
  Object.defineProperty(exports, key, {
    enumerable: true,
    get: reducer[key],
  });
});
