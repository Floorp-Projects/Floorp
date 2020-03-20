/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Makes sure that we report correctly the combo fission + non-webrender

"use strict";

add_task(async function() {
  // Run the check manually. Otherwise, it is deactivated during automation.
  FissionTestingUI.checkFissionWithoutWebRender();

  const isFissionEnabled = Services.prefs.getBoolPref("fission.autostart");
  let isWebRenderEnabled = Cc["@mozilla.org/gfx/info;1"].getService(
    Ci.nsIGfxInfo
  ).WebRenderEnabled;
  const isWarningExpected = isFissionEnabled && !isWebRenderEnabled;

  // Wait until the browser has had a chance to display the warning.
  await gBrowserInit.idleTasksFinishedPromise;

  let isWarningFound = !!gNotificationBox.getNotificationWithValue(
    "warning-fission-without-webrender-notification"
  );
  is(
    isWarningFound,
    isWarningExpected,
    "Did we get the Fission/WebRender warning right?"
  );
});
