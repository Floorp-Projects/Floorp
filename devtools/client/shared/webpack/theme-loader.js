/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var fs = require("fs");

/**
 * Make sure that content of the file is loaded as a text
 * (no parsing by other loaders)
 *
 * Used e.g. for runtime access to colors defined in variables.css
 */
module.exports.pitch = function(remainingRequest, precedingRequest, data) {
  if (this.cacheable) {
    this.cacheable();
  }

  const request = remainingRequest.split("!");
  const rawUrl = request[request.length - 1];
  let content = fs.readFileSync(rawUrl, "utf8");

  // Avoid mix of single & double quotes in a string
  // (use only double quotes), so we can stringify.
  content = content.replace("'", '"');

  return "module.exports = " + JSON.stringify(content) + ";";
};
