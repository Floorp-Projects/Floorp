/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global URLSearchParams, URL */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  function parseURLParams(url) {
    url = new URL(url);
    return parseURLEncodedText(url.searchParams);
  }

  function parseURLEncodedText(text) {
    let params = [];

    // In case the text is empty just return the empty parameters
    if (text == "") {
      return params;
    }

    let searchParams = new URLSearchParams(text);
    let entries = [...searchParams.entries()];
    return entries.map(entry => {
      return {
        name: entry[0],
        value: entry[1]
      };
    });
  }

  // Exports from this module
  exports.parseURLParams = parseURLParams;
  exports.parseURLEncodedText = parseURLEncodedText;
});
