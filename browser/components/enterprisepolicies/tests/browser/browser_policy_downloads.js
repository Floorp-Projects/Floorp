/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(this, "FileUtils",
                               "resource://gre/modules/FileUtils.jsm");

add_task(async function test_defaultdownload() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DefaultDownloadDirectory": "${home}/Downloads",
      "PromptForDownloadLocation": false,
     },
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    is(browser.contentDocument.getElementById("alwaysAsk").disabled, true, "alwaysAsk should be disabled.");
    is(browser.contentDocument.getElementById("saveTo").selected, true, "saveTo should be selected.");
    is(browser.contentDocument.getElementById("saveTo").disabled, true, "saveTo should be disabled.");
    let home = FileUtils.getFile("Home", []).path;
    is(Services.prefs.getStringPref("browser.download.dir"), home + "/Downloads", "browser.download.dir should be ${home}/Downloads.");
    is(Services.prefs.getBoolPref("browser.download.useDownloadDir"), true, "browser.download.useDownloadDir should be true.");
    is(Services.prefs.prefIsLocked("browser.download.useDownloadDir"), true, "browser.download.useDownloadDir should be locked.");
  });
});

add_task(async function test_download() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DownloadDirectory": "${home}/Documents",
     },
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    is(browser.contentDocument.getElementById("alwaysAsk").disabled, true, "alwaysAsk should be disabled.");
    is(browser.contentDocument.getElementById("saveTo").selected, true, "saveTo should be selected.");
    is(browser.contentDocument.getElementById("saveTo").disabled, true, "saveTo should be disabled.");
    is(browser.contentDocument.getElementById("downloadFolder").disabled, true, "downloadFolder should be disabled.");
    is(browser.contentDocument.getElementById("chooseFolder").disabled, true, "chooseFolder should be disabled.");
    let home = FileUtils.getFile("Home", []).path;
    is(Services.prefs.getStringPref("browser.download.dir"), home + "/Documents", "browser.download.dir should be ${home}/Documents.");
    is(Services.prefs.getBoolPref("browser.download.useDownloadDir"), true, "browser.download.useDownloadDir should be true.");
    is(Services.prefs.prefIsLocked("browser.download.useDownloadDir"), true, "browser.download.useDownloadDir should be locked.");
  });
});
