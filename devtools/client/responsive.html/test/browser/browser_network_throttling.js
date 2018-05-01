/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const throttlingProfiles = require("devtools/client/shared/components/throttling/profiles");

// Tests changing network throttling
const TEST_URL = "data:text/html;charset=utf-8,Network throttling test";

addRDMTask(TEST_URL, async function({ ui, manager }) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added
  await waitUntilState(store, state => state.viewports.length == 1);

  // Test defaults
  testNetworkThrottlingSelectorLabel(ui, "No throttling");
  await testNetworkThrottlingState(ui, null);

  // Test a fast profile
  await testThrottlingProfile(ui, "Wi-Fi");

  // Test a slower profile
  await testThrottlingProfile(ui, "Regular 3G");

  // Test switching back to no throttling
  await selectNetworkThrottling(ui, "No throttling");
  testNetworkThrottlingSelectorLabel(ui, "No throttling");
  await testNetworkThrottlingState(ui, null);
});

function testNetworkThrottlingSelectorLabel(ui, expected) {
  let selector = "#global-network-throttling-selector";
  let select = ui.toolWindow.document.querySelector(selector);
  is(select.selectedOptions[0].textContent, expected,
    `Select label should be changed to ${expected}`);
}

var testNetworkThrottlingState = async function(ui, expected) {
  let state = await ui.emulationFront.getNetworkThrottling();
  Assert.deepEqual(state, expected, "Network throttling state should be " +
                                    JSON.stringify(expected, null, 2));
};

var testThrottlingProfile = async function(ui, profile) {
  await selectNetworkThrottling(ui, profile);
  testNetworkThrottlingSelectorLabel(ui, profile);
  let data = throttlingProfiles.find(({ id }) => id == profile);
  let { download, upload, latency } = data;
  await testNetworkThrottlingState(ui, {
    downloadThroughput: download,
    uploadThroughput: upload,
    latency,
  });
};
