/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const actionTypes = {
  MESSAGE_ADD: "MESSAGE_ADD",
  MESSAGES_CLEAR: "MESSAGES_CLEAR",
};

const categories = {
  CATEGORY_NETWORK: "network",
  CATEGORY_CSS: "cssparser",
  CATEGORY_JS: "exception",
  CATEGORY_WEBDEV: "console",
  CATEGORY_INPUT: "input",
  CATEGORY_OUTPUT: "output",
  CATEGORY_SECURITY: "security",
  CATEGORY_SERVER: "server"
};

const severities = {
  SEVERITY_ERROR: "error",
  SEVERITY_WARNING: "warn",
  SEVERITY_INFO: "info",
  SEVERITY_LOG: "log"
};

// A mapping from the console API log event levels to the Web Console
// severities.
const levels = {
  LEVELS: {
    error: severities.SEVERITY_ERROR,
    exception: severities.SEVERITY_ERROR,
    assert: severities.SEVERITY_ERROR,
    warn: severities.SEVERITY_WARNING,
    info: severities.SEVERITY_INFO,
    log: severities.SEVERITY_LOG,
    clear: severities.SEVERITY_LOG,
    trace: severities.SEVERITY_LOG,
    table: severities.SEVERITY_LOG,
    debug: severities.SEVERITY_LOG,
    dir: severities.SEVERITY_LOG,
    dirxml: severities.SEVERITY_LOG,
    group: severities.SEVERITY_LOG,
    groupCollapsed: severities.SEVERITY_LOG,
    groupEnd: severities.SEVERITY_LOG,
    time: severities.SEVERITY_LOG,
    timeEnd: severities.SEVERITY_LOG,
    count: severities.SEVERITY_LOG
  }
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
    COMMAND: "command"
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
module.exports = Object.assign({}, actionTypes, categories, severities, levels,
  chromeRDPEnums);
