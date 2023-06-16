/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const UCT_URI = "chrome://mozapps/content/downloads/unknownContentType.xhtml";

add_task(async function test_defaultdownload() {
  await setupPolicyEngineWithJson({
    policies: {
      DefaultDownloadDirectory: "${home}/Downloads",
      PromptForDownloadLocation: false,
    },
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    is(
      browser.contentDocument.getElementById("alwaysAsk").disabled,
      true,
      "alwaysAsk should be disabled."
    );
    let home = Services.dirsvc.get("Home", Ci.nsIFile).path;
    is(
      Services.prefs.getStringPref("browser.download.dir"),
      home + "/Downloads",
      "browser.download.dir should be ${home}/Downloads."
    );
    is(
      Services.prefs.getBoolPref("browser.download.useDownloadDir"),
      true,
      "browser.download.useDownloadDir should be true."
    );
    is(
      Services.prefs.prefIsLocked("browser.download.useDownloadDir"),
      true,
      "browser.download.useDownloadDir should be locked."
    );
  });
});

add_task(async function test_download() {
  await setupPolicyEngineWithJson({
    policies: {
      DownloadDirectory: "${home}/Documents",
    },
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    is(
      browser.contentDocument.getElementById("alwaysAsk").disabled,
      true,
      "alwaysAsk should be disabled."
    );
    is(
      browser.contentDocument.getElementById("downloadFolder").disabled,
      true,
      "downloadFolder should be disabled."
    );
    is(
      browser.contentDocument.getElementById("chooseFolder").disabled,
      true,
      "chooseFolder should be disabled."
    );
    let home = Services.dirsvc.get("Home", Ci.nsIFile).path;
    is(
      Services.prefs.getStringPref("browser.download.dir"),
      home + "/Documents",
      "browser.download.dir should be ${home}/Documents."
    );
    is(
      Services.prefs.getBoolPref("browser.download.useDownloadDir"),
      true,
      "browser.download.useDownloadDir should be true."
    );
    is(
      Services.prefs.prefIsLocked("browser.download.useDownloadDir"),
      true,
      "browser.download.useDownloadDir should be locked."
    );
  });
});

async function setDownloadDir() {
  let tmpDir = PathUtils.join(
    PathUtils.tempDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function () {
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
  return tmpDir;
}

add_task(async function test_tmpdir_download() {
  await setupPolicyEngineWithJson({
    policies: {
      StartDownloadsInTempDirectory: true,
    },
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", true],
      ["browser.helperApps.deleteTempFileOnExit", true],
    ],
  });

  let dlDir = new FileUtils.File(await setDownloadDir());
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("browser.download.folderList");
  });

  // Wait for the download prompting dialog
  let dialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded(
    null,
    win => win.document.documentURI == UCT_URI
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com/browser/browser/components/downloads/test/browser/foo.txt",
      waitForLoad: false,
      waitForStop: true,
    },
    async function () {
      let dialogWin = await dialogPromise;
      let tempFile = dialogWin.dialog.mLauncher.targetFile;
      isnot(
        tempFile.parent.path,
        dlDir.path,
        "Should not have put temp file in the downloads dir."
      );

      dialogWin.close();
    }
  );
});
