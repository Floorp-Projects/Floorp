/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_fc_downloadOptIn.js

"use strict";

let params = { queryString: "&invalidCompleteSize=1" };

let downloadInfo = [];
if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED, false)) {
  downloadInfo[0] = { patchType: "partial", bitsResult: "0" };
} else {
  downloadInfo[0] = { patchType: "partial", internalResult: "0" };
}

let preSteps = [
  {
    panelId: "checkingForUpdates",
    checkActiveUpdate: null,
    continueFile: CONTINUE_CHECK,
  },
  {
    panelId: "downloadAndInstall",
    checkActiveUpdate: null,
    continueFile: null,
  },
];

let postSteps = [
  {
    panelId: "downloading",
    checkActiveUpdate: { state: STATE_DOWNLOADING },
    continueFile: CONTINUE_DOWNLOAD,
    downloadInfo,
  },
  {
    panelId: "apply",
    checkActiveUpdate: { state: STATE_PENDING },
    continueFile: null,
  },
];

add_task(async function test() {
  // Disable the pref that automatically downloads and installs updates.
  await UpdateUtils.setAppUpdateAutoEnabled(false);

  await initUpdate(params);

  let ext = await loadExtension(async () => {
    browser.test.onMessage.addListener(async command => {
      switch (command) {
        case "check":
          await browser.experiments.urlbar.checkForBrowserUpdate();
          browser.test.sendMessage("done");
          break;
        case "install":
          let done = false;
          let interval = setInterval(async () => {
            let status = await browser.experiments.urlbar.getBrowserUpdateStatus();
            if (status == "downloadAndInstall" && !done) {
              done = true;
              clearInterval(interval);
              browser.test.withHandlingUserInput(async () => {
                browser.experiments.urlbar.installBrowserUpdateAndRestart();
                browser.test.sendMessage("done", status);
              });
            }
          }, 100);
          break;
      }
    });
  });

  ext.sendMessage("check");
  await ext.awaitMessage("done");

  // Set up the "download and install" update state.
  await processUpdateSteps(preSteps);

  ext.sendMessage("install");
  await ext.awaitMessage("done");

  await Promise.all([
    processUpdateSteps(postSteps),
    TestUtils.topicObserved(
      "quit-application-requested",
      (cancelQuit, data) => {
        if (data == "restart") {
          cancelQuit.QueryInterface(Ci.nsISupportsPRBool).data = true;
          return true;
        }
        return false;
      }
    ),
  ]);
  Assert.ok(true, "Restart attempted");

  await ext.unload();
});
