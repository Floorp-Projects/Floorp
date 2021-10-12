ChromeUtils.defineModuleGetter(
  this,
  "Downloads",
  "resource://gre/modules/Downloads.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadsCommon",
  "resource:///modules/DownloadsCommon.jsm"
);

let INSECURE_BASE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://example.com/"
  ) + "download_page.html";
let SECURE_BASE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  ) + "download_page.html";

function promiseFocus() {
  return new Promise(resolve => {
    waitForFocus(resolve);
  });
}

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

async function task_openPanel() {
  await promiseFocus();

  let promise = promisePanelOpened();
  DownloadsPanel.showPanel();
  await promise;
}

/**
 * Waits until the download Prompt is shown, selects
 * the choosen <action>, then accepts the dialog
 * @param [action] Which action to select, either:
 *        "handleInternally", "save" or "open".
 * @returns {Promise} Resolved once done.
 */

function shouldPromptDownload(action = "save") {
  return new Promise((resolve, reject) => {
    Services.wm.addListener({
      onOpenWindow(xulWin) {
        Services.wm.removeListener(this);
        let win = xulWin.docShell.domWindow;
        waitForFocus(() => {
          if (
            win.location ==
            "chrome://mozapps/content/downloads/unknownContentType.xhtml"
          ) {
            let dialog = win.document.getElementById("unknownContentType");
            let button = dialog.getButton("accept");
            let actionRadio = win.document.getElementById(action);
            actionRadio.click();
            button.disabled = false;
            dialog.acceptDialog();
            resolve();
          } else {
            reject();
          }
        }, win);
      },
    });
  });
}

const CONSOLE_ERROR_MESSAGE = "Blocked downloading insecure content";

function shouldConsoleError() {
  // Waits until CONSOLE_ERROR_MESSAGE was logged
  return new Promise((resolve, reject) => {
    function listener(msgObj) {
      let text = msgObj.message;
      if (text.includes(CONSOLE_ERROR_MESSAGE)) {
        Services.console.unregisterListener(listener);
        resolve();
      }
    }
    Services.console.registerListener(listener);
  });
}

async function resetDownloads() {
  // Removes all downloads from the download List
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    publicList.remove(download);
    await download.finalize(true);
  }
}

async function shouldNotifyDownloadUI() {
  // Waits until a Blocked download was added to the Download List
  // -> returns the blocked Download
  let list = await Downloads.getList(Downloads.ALL);
  return new Promise((res, err) => {
    const view = {
      onDownloadAdded: async aDownload => {
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
          // Assert that the Referrer is presnt
          if (!aDownload.source.referrerInfo) {
            throw new Error("The Blocked download is missing the ReferrerInfo");
          }

          res(aDownload);
          list.removeView(view);
        }
      },
    };
    list.addView(view);
  });
}

async function runTest(url, link, checkFunction, description) {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.block_download_insecure", true]],
  });
  await resetDownloads();

  let tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Checking: " + description);

  let checkPromise = checkFunction();
  // Click the Link to trigger the download
  SpecialPowers.spawn(gBrowser.selectedBrowser, [link], contentLink => {
    content.document.getElementById(contentLink).click();
  });

  await checkPromise;

  ok(true, description);
  BrowserTestUtils.removeTab(tab);

  await SpecialPowers.popPrefEnv();
}

// Test Blocking
add_task(async function() {
  await runTest(
    INSECURE_BASE_URL,
    "insecure",
    shouldPromptDownload,
    "Insecure -> Insecure should download"
  );
  await runTest(
    INSECURE_BASE_URL,
    "secure",
    shouldPromptDownload,
    "Insecure -> Secure should download"
  );
  await runTest(
    SECURE_BASE_URL,
    "insecure",
    () =>
      Promise.all([
        shouldPromptDownload(),
        shouldNotifyDownloadUI(),
        shouldConsoleError(),
      ]),
    "Secure -> Insecure should Error"
  );
  await runTest(
    SECURE_BASE_URL,
    "secure",
    shouldPromptDownload,
    "Secure -> Secure should Download"
  );
});
// Test Manual Unblocking
add_task(async function() {
  await runTest(
    SECURE_BASE_URL,
    "insecure",
    async () => {
      await shouldPromptDownload();
      let download = await shouldNotifyDownloadUI();
      await download.unblock();
      ok(download.error == null, "There should be no error after unblocking");
    },
    "A Blocked Download Should succeeded to Download after a Manual unblock"
  );
});

// Test Unblock Download Visible
add_task(async function() {
  // Focus, open and close the panel once
  // to make sure the panel is loaded and ready
  await promiseFocus();
  await runTest(
    SECURE_BASE_URL,
    "insecure",
    async () => {
      let panelHasOpened = promisePanelOpened();
      info("awaiting that the Download Prompt is shown");
      await shouldPromptDownload();
      info("awaiting that the Download list adds the new download");
      await shouldNotifyDownloadUI();
      info("awaiting that the Download list shows itself");
      await panelHasOpened;
      DownloadsPanel.hidePanel();
      ok(true, "The Download Panel should have opened on blocked download");
    },
    "A Blocked Download Should open the Download Panel"
  );
});

// Test Download an insecure pdf and choose "Open with Firefox"
add_task(async function download_open_insecure_pdf() {
  await promiseFocus();
  await runTest(
    SECURE_BASE_URL,
    "insecurePDF",
    async () => {
      info("awaiting that the Download Prompt is shown");
      await shouldPromptDownload("handleInternally");
      let download = await shouldNotifyDownloadUI();
      let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
      await download.unblock();
      ok(download.error == null, "There should be no error after unblocking");
      let tab = await newTabPromise;

      // Fix (possible) windows file delemiters
      let usablePath = download.target.path.replace("\\", "/");
      ok(
        tab.linkedBrowser._documentURI.filePath.includes(".pdf"),
        "The download target was opened"
      );
      BrowserTestUtils.removeTab(tab);
      ok(true, "The Content was opened in a new tab");
    },
    "A Blocked PDF can be opened internally"
  );
});
