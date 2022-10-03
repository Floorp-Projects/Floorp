/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  SessionDataHelpers,
} = require("resource://devtools/server/actors/watcher/SessionDataHelpers.jsm");
const { SUPPORTED_DATA } = SessionDataHelpers;

const SessionDataProcessors = {};

loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.BLACKBOXING,
  "resource://devtools/server/actors/targets/session-data-processors/blackboxing.js"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.BREAKPOINTS,
  "resource://devtools/server/actors/targets/session-data-processors/breakpoints.js"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.EVENT_BREAKPOINTS,
  "resource://devtools/server/actors/targets/session-data-processors/event-breakpoints.js"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.RESOURCES,
  "resource://devtools/server/actors/targets/session-data-processors/resources.js"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.TARGET_CONFIGURATION,
  "resource://devtools/server/actors/targets/session-data-processors/target-configuration.js"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.THREAD_CONFIGURATION,
  "resource://devtools/server/actors/targets/session-data-processors/thread-configuration.js"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.XHR_BREAKPOINTS,
  "resource://devtools/server/actors/targets/session-data-processors/xhr-breakpoints.js"
);

exports.SessionDataProcessors = SessionDataProcessors;
