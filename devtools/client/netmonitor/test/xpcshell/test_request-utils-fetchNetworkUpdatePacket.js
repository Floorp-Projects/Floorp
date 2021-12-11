/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test devtools/client/netmonitor/src/utils/request-utils.js function
// |fetchNetworkUpdatePacket|

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");

function run_test() {
  let fetchedNetworkUpdateTypes = [];
  async function mockConnectorRequestData(id, updateType) {
    fetchedNetworkUpdateTypes.push(updateType);
    return updateType;
  }

  const updateTypes = ["requestHeaders", "responseHeaders"];
  const request = {
    id: "foo",
    requestHeadersAvailable: true,
    responseHeadersAvailable: true,
  };

  info(
    "Testing that the expected network update packets were fetched from the server"
  );
  fetchNetworkUpdatePacket(mockConnectorRequestData, request, updateTypes);
  equal(fetchedNetworkUpdateTypes.length, 2, "Two network request updates");
  equal(
    fetchedNetworkUpdateTypes[0],
    "requestHeaders",
    "Request headers updates"
  );
  equal(
    fetchedNetworkUpdateTypes[1],
    "responseHeaders",
    " Response headers updates"
  );

  // clear the fetch updates for next test
  fetchedNetworkUpdateTypes = [];

  info(
    "Testing that no network updates were fetched when no `request` is provided"
  );
  fetchNetworkUpdatePacket(mockConnectorRequestData, undefined, updateTypes);
  equal(fetchedNetworkUpdateTypes.length, 0, "No network request updates");
}
