/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  profiles,
} = require("resource://devtools/client/shared/components/throttling/profiles.js");

// Tests changing network throttling
const TEST_URL = "data:text/html;charset=utf-8,Network throttling test";

addRDMTask(TEST_URL, async function ({ ui, manager }) {
  // Test defaults
  testNetworkThrottlingSelectorLabel(ui, "No Throttling", "No Throttling");
  await testNetworkThrottlingState(ui, null);

  // Test a fast profile
  await testThrottlingProfile(
    ui,
    "Wi-Fi",
    "download 30Mbps, upload 15Mbps, latency 2ms"
  );

  // Test a slower profile
  await testThrottlingProfile(
    ui,
    "Regular 3G",
    "download 750Kbps, upload 250Kbps, latency 100ms"
  );

  // Test switching back to no throttling
  await selectNetworkThrottling(ui, "No Throttling");
  testNetworkThrottlingSelectorLabel(ui, "No Throttling", "No Throttling");
  await testNetworkThrottlingState(ui, null);
});

function testNetworkThrottlingSelectorLabel(
  ui,
  expectedLabel,
  expectedTooltip
) {
  const title = ui.toolWindow.document.querySelector(
    "#network-throttling-menu .title"
  );
  is(
    title.textContent,
    expectedLabel,
    `Button label should be changed to ${expectedLabel}`
  );
  is(
    title.parentNode.getAttribute("title"),
    expectedTooltip,
    `Button tooltip should be changed to ${expectedTooltip}`
  );
}

var testNetworkThrottlingState = async function (ui, expected) {
  const state = await ui.networkFront.getNetworkThrottling();
  Assert.deepEqual(
    state,
    expected,
    "Network throttling state should be " + JSON.stringify(expected, null, 2)
  );
};

var testThrottlingProfile = async function (ui, profile, tooltip) {
  await selectNetworkThrottling(ui, profile);
  testNetworkThrottlingSelectorLabel(ui, profile, tooltip);
  const data = profiles.find(({ id }) => id == profile);
  const { download, upload, latency } = data;
  await testNetworkThrottlingState(ui, {
    downloadThroughput: download,
    uploadThroughput: upload,
    latency,
  });
};
