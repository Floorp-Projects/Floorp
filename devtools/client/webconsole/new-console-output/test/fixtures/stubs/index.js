/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let maps = [];

[
  "consoleApi",
  "evaluationResult",
  "pageError",
].forEach((filename) => {
  maps[filename] = require(`./${filename}`);
});

// Combine all the maps into a single map.
module.exports = new Map([
  ...maps.consoleApi,
  ...maps.evaluationResult,
  ...maps.pageError,
]);

