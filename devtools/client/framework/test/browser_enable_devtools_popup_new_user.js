/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper_enable_devtools_popup.js */
loadHelperScript("helper_enable_devtools_popup.js");

const TEST_URL =
  "data:text/html,<html><head><title>Test Disable F12 experiment</title></head><body>" +
  "<h1>Disable F12 experiment</h1></body></html>";

// See the corresponding browser-enable-popup-devtools-user.ini for the
// initialization of the prefs:
// - devtools.experiment.f12.shortcut_disabled -> true
// - devtools.selfxss.count -> 0
//
// Those prefs are set in the browser ini to run before DevToolsStartup.jsm init
// logic. We expect devtools.selfxss.count to force shortcut_disabled to false.
add_task(async function() {
  const tab = await addTab(TEST_URL);
  await new Promise(done => waitForFocus(done));

  // With the shortcut initially disabled and the selfxss pref at 0, we expect
  // the shortcut to be effectively disabled.
  await checkF12IsDisabled(tab);
  const toolbox = await openDevToolsWithInspectorKey(tab);
  await closeDevToolsWithF12(tab, toolbox);
  await checkF12IsEnabled(tab);
});
