/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const maps = [];

[
  "consoleApi",
  "cssMessage",
  "evaluationResult",
  "networkEvent",
  "pageError",
].forEach(filename => {
  maps[filename] = require(`./${filename}`);
});

// Combine all the maps into a single map.
module.exports = {
  stubPreparedMessages: new Map([
    ...maps.consoleApi.stubPreparedMessages,
    ...maps.cssMessage.stubPreparedMessages,
    ...maps.evaluationResult.stubPreparedMessages,
    ...maps.networkEvent.stubPreparedMessages,
    ...maps.pageError.stubPreparedMessages,
  ]),
  stubPackets: new Map([
    ...maps.consoleApi.stubPackets,
    ...maps.cssMessage.stubPackets,
    ...maps.evaluationResult.stubPackets,
    ...maps.networkEvent.stubPackets,
    ...maps.pageError.stubPackets,
  ]),
};
