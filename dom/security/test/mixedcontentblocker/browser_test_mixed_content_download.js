ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  DownloadsCommon: "resource:///modules/DownloadsCommon.sys.mjs",
});

const HandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);

const MIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

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

/**
 * Waits until a download is triggered.
 * Unless the always_ask_before_handling_new_types pref is true, the download
 * will simply be saved, so resolve when the view is notified of the new
 * download. Otherwise, it waits until a prompt is shown, selects the choosen
 * <action>, then accepts the dialog
 * @param [action] Which action to select, either:
 *        "handleInternally", "save" or "open".
 * @returns {Promise} Resolved once done.
 */

function shouldTriggerDownload(action = "save") {
  if (
    Services.prefs.getBoolPref(
      "browser.download.always_ask_before_handling_new_types"
    )
  ) {
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
  return new Promise(res => {
    downloadMonitoringView.waitForDownload(res);
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
  const types = new Set();
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    if (download.contentType) {
      types.add(download.contentType);
    }
    publicList.remove(download);
    await download.finalize(true);
  }

  if (types.size) {
    // reset handlers for the contentTypes of any files previously downloaded
    for (let type of types) {
      const mimeInfo = MIMEService.getFromTypeAndExtension(type, "");
      info("resetting handler for type: " + type);
      HandlerService.remove(mimeInfo);
    }
  }
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
        // Assert that the Referrer is presnt
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

add_setup(async () => {
  let list = await Downloads.getList(Downloads.ALL);
  list.addView(downloadMonitoringView);
  registerCleanupFunction(() => list.removeView(downloadMonitoringView));
});

// Test Blocking
add_task(async function test_blocking() {
  for (let prefVal of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.download.always_ask_before_handling_new_types", prefVal]],
    });
    await runTest(
      INSECURE_BASE_URL,
      "insecure",
      shouldTriggerDownload,
      "Insecure -> Insecure should download"
    );
    await runTest(
      INSECURE_BASE_URL,
      "secure",
      shouldTriggerDownload,
      "Insecure -> Secure should download"
    );
    await runTest(
      SECURE_BASE_URL,
      "insecure",
      () =>
        Promise.all([
          shouldTriggerDownload(),
          shouldNotifyDownloadUI(),
          shouldConsoleError(),
        ]),
      "Secure -> Insecure should Error"
    );
    await runTest(
      SECURE_BASE_URL,
      "secure",
      shouldTriggerDownload,
      "Secure -> Secure should Download"
    );
    await SpecialPowers.popPrefEnv();
  }
});

// Test Manual Unblocking
add_task(async function test_manual_unblocking() {
  for (let prefVal of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.download.always_ask_before_handling_new_types", prefVal]],
    });
    await runTest(
      SECURE_BASE_URL,
      "insecure",
      async () => {
        let [, download] = await Promise.all([
          shouldTriggerDownload(),
          shouldNotifyDownloadUI(),
        ]);
        await download.unblock();
        ok(download.error == null, "There should be no error after unblocking");
      },
      "A Blocked Download Should succeeded to Download after a Manual unblock"
    );
    await SpecialPowers.popPrefEnv();
  }
});

// Test Unblock Download Visible
add_task(async function test_unblock_download_visible() {
  for (let prefVal of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.download.always_ask_before_handling_new_types", prefVal]],
    });
    // Focus, open and close the panel once
    // to make sure the panel is loaded and ready
    await promiseFocus();
    await runTest(
      SECURE_BASE_URL,
      "insecure",
      async () => {
        let panelHasOpened = promisePanelOpened();
        info("awaiting that the download is triggered and added to the list");
        await Promise.all([shouldTriggerDownload(), shouldNotifyDownloadUI()]);
        info("awaiting that the Download list shows itself");
        await panelHasOpened;
        DownloadsPanel.hidePanel();
        ok(true, "The Download Panel should have opened on blocked download");
      },
      "A Blocked Download Should open the Download Panel"
    );
    await SpecialPowers.popPrefEnv();
  }
});

// Test Download an insecure svg and choose "Open with Firefox"
add_task(async function download_open_insecure_SVG() {
  const mimeInfo = MIMEService.getFromTypeAndExtension("image/svg+xml", "svg");
  mimeInfo.alwaysAskBeforeHandling = false;
  mimeInfo.preferredAction = mimeInfo.handleInternally;
  HandlerService.store(mimeInfo);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", false]],
  });
  await promiseFocus();
  await runTest(
    SECURE_BASE_URL,
    "insecureSVG",
    async () => {
      info("awaiting that the download is triggered and added to the list");
      let [_, download] = await Promise.all([
        shouldTriggerDownload("handleInternally"),
        shouldNotifyDownloadUI(),
      ]);

      let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
      await download.unblock();
      ok(download.error == null, "There should be no error after unblocking");

      let tab = await newTabPromise;

      ok(
        tab.linkedBrowser._documentURI.filePath.includes(".svg"),
        "The download target was opened"
      );
      BrowserTestUtils.removeTab(tab);
      ok(true, "The Content was opened in a new tab");
      await SpecialPowers.popPrefEnv();
    },
    "A Blocked SVG can be opened internally"
  );

  HandlerService.remove(mimeInfo);
});
