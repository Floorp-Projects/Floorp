/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the previous viewport size, user agent, dppx and touch simulation properties
// are restored when reopening RDM.

const TEST_URL = "data:text/html;charset=utf-8,";
const DEFAULT_DPPX = window.devicePixelRatio;
const NEW_DPPX = DEFAULT_DPPX + 1;
const NEW_USER_AGENT = "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0";

addRDMTask(TEST_URL, async function({ ui, manager }) {
  const { store } = ui.toolWindow;

  reloadOnTouchChange(true);
  reloadOnUAChange(true);

  await waitUntilState(store, state => state.viewports.length == 1);

  info("Checking the default RDM state.");
  testViewportDeviceMenuLabel(ui, "Responsive");
  testViewportDimensions(ui, 320, 480);
  await testUserAgent(ui, DEFAULT_UA);
  await testDevicePixelRatio(ui, DEFAULT_DPPX);
  await testTouchEventsOverride(ui, false);

  info("Changing the RDM size, dppx, ua and toggle ON touch simulations.");
  await setViewportSize(ui, manager, 90, 500);
  await selectDevicePixelRatio(ui, NEW_DPPX);
  await toggleTouchSimulation(ui);
  await changeUserAgentInput(ui, NEW_USER_AGENT);

  reloadOnTouchChange(false);
  reloadOnUAChange(false);
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { store } = ui.toolWindow;

  reloadOnTouchChange(true);
  reloadOnUAChange(true);

  info("Reopening RDM and checking that the previous state is restored.");

  await waitUntilState(store, state => state.viewports.length == 1);

  testViewportDimensions(ui, 90, 500);
  await testUserAgent(ui, NEW_USER_AGENT);
  await testDevicePixelRatio(ui, NEW_DPPX);
  await testTouchEventsOverride(ui, true);

  info("Rotating the viewport.");
  rotateViewport(ui);

  reloadOnTouchChange(false);
  reloadOnUAChange(false);
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { store } = ui.toolWindow;

  reloadOnTouchChange(true);
  reloadOnUAChange(true);

  info("Reopening RDM and checking that the previous state is restored.");

  await waitUntilState(store, state => state.viewports.length == 1);

  testViewportDimensions(ui, 500, 90);
  await testUserAgent(ui, NEW_USER_AGENT);
  await testDevicePixelRatio(ui, NEW_DPPX);
  await testTouchEventsOverride(ui, true);

  reloadOnTouchChange(false);
  reloadOnUAChange(false);
});
