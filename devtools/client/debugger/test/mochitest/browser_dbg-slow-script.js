/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests the slow script warning

add_task(async function openDebuggerFirst() {
  // In mochitest, the timeout is disable, so set it to a short, but non zero duration
  await pushPref("dom.max_script_run_time", 1);
  // Prevents having to click on the page to have the dialog to appear
  await pushPref("dom.max_script_run_time.require_critical_input", false);

  const dbg = await initDebugger("doc-slow-script.html");

  const alert = BrowserTestUtils.waitForGlobalNotificationBar(
    window,
    "process-hang"
  );

  info("Execute an infinite loop");
  invokeInTab("infiniteLoop");

  info("Wait for the slow script warning");
  const notification = await alert;

  info("Click on the debug script button");
  const buttons = notification.buttonContainer.getElementsByTagName("button");
  // The first button is "stop", the second is "debug script"
  buttons[1].click();

  info("Waiting for the debugger to be paused");
  await waitForPaused(dbg);
  const source = findSource(dbg, "doc-slow-script.html");
  assertPausedAtSourceAndLine(dbg, source.id, 14);

  info("Close toolbox and tab");
  await dbg.toolbox.closeToolbox();
  await removeTab(gBrowser.selectedTab);
});
