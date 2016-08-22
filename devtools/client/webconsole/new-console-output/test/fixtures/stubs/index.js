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
module.exports = {
    stubPreparedMessages: new Map([
      ...maps.consoleApi.stubPreparedMessages,
      ...maps.evaluationResult.stubPreparedMessages,
      ...maps.pageError.stubPreparedMessages, ]),
    stubPackets: new Map([
      ...maps.consoleApi.stubPackets,
      ...maps.evaluationResult.stubPackets,
      ...maps.pageError.stubPackets, ]),
};
