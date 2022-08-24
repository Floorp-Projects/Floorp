/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  SessionDataHelpers,
} = require("devtools/server/actors/watcher/SessionDataHelpers.jsm");
const { SUPPORTED_DATA } = SessionDataHelpers;

const SessionDataProcessors = {};

loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.BLACKBOXING,
  "devtools/server/actors/targets/session-data-processors/blackboxing"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.BREAKPOINTS,
  "devtools/server/actors/targets/session-data-processors/breakpoints"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.EVENT_BREAKPOINTS,
  "devtools/server/actors/targets/session-data-processors/event-breakpoints"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.RESOURCES,
  "devtools/server/actors/targets/session-data-processors/resources"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.TARGET_CONFIGURATION,
  "devtools/server/actors/targets/session-data-processors/target-configuration"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.THREAD_CONFIGURATION,
  "devtools/server/actors/targets/session-data-processors/thread-configuration"
);
loader.lazyRequireGetter(
  SessionDataProcessors,
  SUPPORTED_DATA.XHR_BREAKPOINTS,
  "devtools/server/actors/targets/session-data-processors/xhr-breakpoints"
);

exports.SessionDataProcessors = SessionDataProcessors;
