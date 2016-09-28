/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const actionTypes = {
  BATCH_ACTIONS: "BATCH_ACTIONS",
  MESSAGE_ADD: "MESSAGE_ADD",
  MESSAGES_CLEAR: "MESSAGES_CLEAR",
  MESSAGE_OPEN: "MESSAGE_OPEN",
  MESSAGE_CLOSE: "MESSAGE_CLOSE",
  MESSAGE_TABLE_RECEIVE: "MESSAGE_TABLE_RECEIVE",
  FILTER_TOGGLE: "FILTER_TOGGLE",
  FILTER_TEXT_SET: "FILTER_TEXT_SET",
  FILTERS_CLEAR: "FILTERS_CLEAR",
  FILTER_BAR_TOGGLE: "FILTER_BAR_TOGGLE",
};

const chromeRDPEnums = {
  MESSAGE_SOURCE: {
    XML: "xml",
    JAVASCRIPT: "javascript",
    NETWORK: "network",
    CONSOLE_API: "console-api",
    STORAGE: "storage",
    APPCACHE: "appcache",
    RENDERING: "rendering",
    SECURITY: "security",
    OTHER: "other",
    DEPRECATION: "deprecation"
  },
  MESSAGE_TYPE: {
    LOG: "log",
    DIR: "dir",
    TABLE: "table",
    TRACE: "trace",
    CLEAR: "clear",
    START_GROUP: "startGroup",
    START_GROUP_COLLAPSED: "startGroupCollapsed",
    END_GROUP: "endGroup",
    ASSERT: "assert",
    PROFILE: "profile",
    PROFILE_END: "profileEnd",
    // Undocumented in Chrome RDP, but is used for evaluation results.
    RESULT: "result",
    // Undocumented in Chrome RDP, but is used for input.
    COMMAND: "command",
    // Undocumented in Chrome RDP, but is used for messages that should not
    // output anything (e.g. `console.time()` calls).
    NULL_MESSAGE: "nullMessage",
  },
  MESSAGE_LEVEL: {
    LOG: "log",
    ERROR: "error",
    WARN: "warn",
    DEBUG: "debug",
    INFO: "info"
  }
};

// Combine into a single constants object
module.exports = Object.assign({}, actionTypes, chromeRDPEnums);
