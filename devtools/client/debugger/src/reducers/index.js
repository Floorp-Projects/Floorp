/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Reducer index
 * @module reducers/index
 */

import expressions from "./expressions";
import sourceActors from "./source-actors";
import sources from "./sources";
import tabs from "./tabs";
import breakpoints from "./breakpoints";
import pendingBreakpoints from "./pending-breakpoints";
import asyncRequests from "./async-requests";
import pause from "./pause";
import ui from "./ui";
import fileSearch from "./file-search";
import ast from "./ast";
import preview from "./preview";
import projectTextSearch from "./project-text-search";
import quickOpen from "./quick-open";
import sourceTree from "./source-tree";
import debuggee from "./debuggee";
import eventListenerBreakpoints from "./event-listeners";

// eslint-disable-next-line import/named
import { objectInspector } from "devtools-reps";

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
  debuggee,
  objectInspector: objectInspector.reducer.default,
  eventListenerBreakpoints,
  preview,
};
