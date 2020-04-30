/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper_enable_devtools_popup.js */
loadHelperScript("helper_enable_devtools_popup.js");

const TEST_URL =
  "data:text/html,<html><head><title>Test Disable F12 experiment</title></head><body>" +
  "<h1>Disable F12 experiment</h1></body></html>";

// Test the basic behavior of the enable devtools popup depending on the value
// of the devtools.experiment.f12.shortcut_disabled preference
add_task(async function testWithF12Disabled() {
  await pushPref("devtools.experiment.f12.shortcut_disabled", true);

  const tab = await addTab(TEST_URL);
  await new Promise(done => waitForFocus(done));

  await checkF12IsDisabled(tab);
  const toolbox = await openDevToolsWithInspectorKey(tab);
  await closeDevToolsWithF12(tab, toolbox);
  await checkF12IsEnabled(tab);
});

add_task(async function testWithF12Enabled() {
  await pushPref("devtools.experiment.f12.shortcut_disabled", false);

  const tab = await addTab(TEST_URL);
  await new Promise(done => waitForFocus(done));

  await checkF12IsEnabled(tab);
});
