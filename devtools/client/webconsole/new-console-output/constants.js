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
  CATEGORY_NETWORK: 0,
  CATEGORY_CSS: 1,
  CATEGORY_JS: 2,
  CATEGORY_WEBDEV: 3,
  CATEGORY_INPUT: 4,
  CATEGORY_OUTPUT: 5,
  CATEGORY_SECURITY: 6,
  CATEGORY_SERVER: 7
};

const severities = {
  SEVERITY_ERROR: 0,
  SEVERITY_WARNING: 1,
  SEVERITY_INFO: 2,
  SEVERITY_LOG: 3
};

// The fragment of a CSS class name that identifies categories/severities.
const fragments = {
  CATEGORY_CLASS_FRAGMENTS: [
    "network",
    "cssparser",
    "exception",
    "console",
    "input",
    "output",
    "security",
    "server",
  ],
  SEVERITY_CLASS_FRAGMENTS: [
    "error",
    "warn",
    "info",
    "log",
  ]
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

// Combine into a single constants object
module.exports = Object.assign({}, actionTypes, categories, severities,
  fragments, levels);
