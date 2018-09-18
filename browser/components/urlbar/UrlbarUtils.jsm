/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarUtils singleton, which contains constants and
 * helper functions that are useful to all components of the urlbar.
 */

var EXPORTED_SYMBOLS = ["UrlbarUtils"];

var UrlbarUtils = {
  // Values for browser.urlbar.insertMethod
  INSERTMETHOD: {
    // Just append new results.
    APPEND: 0,
    // Merge previous and current results if search strings are related.
    MERGE_RELATED: 1,
    // Always merge previous and current results.
    MERGE: 2,
  },

  MATCHTYPE: {
    HEURISTIC: "heuristic",
    GENERAL: "general",
    SUGGESTION: "suggestion",
    EXTENSION: "extension",
  },

  // Extensions are allowed to add suggestions if they have registered a keyword
  // with the omnibox API. This is the maximum number of suggestions an extension
  // is allowed to add for a given search string.
  // This value includes the heuristic result.
  MAXIMUM_ALLOWED_EXTENSION_MATCHES: 6,
};
