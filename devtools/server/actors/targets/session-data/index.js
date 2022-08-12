/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  SessionDataHelpers,
} = require("devtools/server/actors/watcher/SessionDataHelpers.jsm");
const { SUPPORTED_DATA } = SessionDataHelpers;

const SessionData = {};

loader.lazyRequireGetter(
  SessionData,
  SUPPORTED_DATA.BLACKBOXING,
  "devtools/server/actors/targets/session-data/blackboxing"
);
loader.lazyRequireGetter(
  SessionData,
  SUPPORTED_DATA.BREAKPOINTS,
  "devtools/server/actors/targets/session-data/breakpoints"
);
loader.lazyRequireGetter(
  SessionData,
  SUPPORTED_DATA.EVENT_BREAKPOINTS,
  "devtools/server/actors/targets/session-data/event-breakpoints"
);
loader.lazyRequireGetter(
  SessionData,
  SUPPORTED_DATA.RESOURCES,
  "devtools/server/actors/targets/session-data/resources"
);
loader.lazyRequireGetter(
  SessionData,
  SUPPORTED_DATA.TARGET_CONFIGURATION,
  "devtools/server/actors/targets/session-data/target-configuration"
);
loader.lazyRequireGetter(
  SessionData,
  SUPPORTED_DATA.THREAD_CONFIGURATION,
  "devtools/server/actors/targets/session-data/thread-configuration"
);
loader.lazyRequireGetter(
  SessionData,
  SUPPORTED_DATA.XHR_BREAKPOINTS,
  "devtools/server/actors/targets/session-data/xhr-breakpoints"
);

exports.SessionData = SessionData;
