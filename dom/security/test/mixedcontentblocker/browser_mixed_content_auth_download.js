/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  DownloadsCommon: "resource:///modules/DownloadsCommon.sys.mjs",
});

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

const downloadMonitoringView = {
  _listeners: [],
  onDownloadAdded(download) {
    for (let listener of this._listeners) {
      listener(download);
    }
    this._listeners = [];
  },
  waitForDownload(listener) {
    this._listeners.push(listener);
  },
};

let SECURE_BASE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  ) + "file_auth_download_page.html";

/**
 * Waits until a download is triggered.
 * It waits until a prompt is shown,
 * saves and then accepts the dialog.
 * @returns {Promise} Resolved once done.
 */

function shouldTriggerDownload() {
  return new Promise(res => {
    downloadMonitoringView.waitForDownload(res);
  });
}
function shouldNotifyDownloadUI() {
  return new Promise(res => {
    downloadMonitoringView.waitForDownload(async aDownload => {
      let { error } = aDownload;
      if (
        error.becauseBlockedByReputationCheck &&
        error.reputationCheckVerdict == Downloads.Error.BLOCK_VERDICT_INSECURE
      ) {
        // It's an insecure Download, now Check that it has been cleaned up properly
        if ((await IOUtils.stat(aDownload.target.path)).size != 0) {
          throw new Error(`Download target is not empty!`);
        }
        if ((await IOUtils.stat(aDownload.target.path)).size != 0) {
          throw new Error(`Download partFile was not cleaned up properly`);
        }
        // Assert that the Referrer is present
        if (!aDownload.source.referrerInfo) {
          throw new Error("The Blocked download is missing the ReferrerInfo");
        }

        res(aDownload);
      } else {
        ok(false, "No error for download that was expected to error!");
      }
    });
  });
}

async function resetDownloads() {
  // Removes all downloads from the download List
  const types = new Set();
  let publicList = await Downloads.getList(Downloads.ALL);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    if (download.contentType) {
      types.add(download.contentType);
    }
    publicList.remove(download);
    await download.finalize(true);
  }
}

async function runTest(url, link, checkFunction, description) {
  await resetDownloads();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  is(
    gBrowser.currentURI.schemeIs("https"),
    true,
    "Scheme of opened tab should be https"
  );
  info("Checking: " + description);

  let checkPromise = checkFunction();
  // Click the Link to trigger the download
  SpecialPowers.spawn(gBrowser.selectedBrowser, [link], contentLink => {
    content.document.getElementById(contentLink).click();
  });
  // Wait for the auth prompt, enter the login details and close the prompt
  await PromptTestUtils.handleNextPrompt(
    gBrowser.selectedBrowser,
    { modalType: Ci.nsIPrompt.MODAL_TYPE_TAB, promptType: "promptUserAndPass" },
    { buttonNumClick: 0, loginInput: "user", passwordInput: "pass" }
  );
  await checkPromise;
  ok(true, description);
  // Close download panel
  DownloadsPanel.hidePanel();
  is(DownloadsPanel.panel.state, "closed", "Panel should be closed");
  await BrowserTestUtils.removeTab(tab);
}

add_setup(async function () {
  let list = await Downloads.getList(Downloads.ALL);
  list.addView(downloadMonitoringView);
  registerCleanupFunction(() => list.removeView(downloadMonitoringView));
  // Ensure to delete all cached credentials before running test
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.block_download_insecure", true]],
  });
});
//Test description:
// 1. Open "https://example.com".
// 2. From "https://example.com" download something, but that download is only available via http
//    and with authentication.
// 3. Login and start download.
// 4. Mixed-content blocker blocks download.
// 5. Unblock download and verify the downloaded file.
add_task(async function test_auth_download() {
  await runTest(
    SECURE_BASE_URL,
    "insecure",
    async () => {
      let [, download] = await Promise.all([
        shouldTriggerDownload(),
        shouldNotifyDownloadUI(),
      ]);
      await download.unblock();
      Assert.equal(
        download.error,
        null,
        "There should be no error after unblocking"
      );
      info(
        "Start download to be able to validate the size and the success of the download"
      );
      await download.start();
      is(
        download.contentType,
        "text/html",
        "File contentType should be correct."
      );
      ok(download.succeeded, "Download succeeded!");
      is(download.target.size, 27, "Download has correct size");
    },
    "A locked Download from an auth server should succeeded to Download after a Manual unblock"
  );
});
