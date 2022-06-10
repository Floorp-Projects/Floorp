/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that the downloads panel opens when a download is spoofed.
 */
async function checkPanelOpens() {
  info("Waiting for panel to open.");
  let promise = promisePanelOpened();
  DownloadsCommon.getData(window)._notifyDownloadEvent("start");
  is(
    DownloadsPanel.isPanelShowing,
    true,
    "Panel state should indicate a preparation to be opened."
  );
  await promise;

  is(DownloadsPanel.panel.state, "open", "Panel should be opened.");

  DownloadsPanel.hidePanel();
}

/**
 * Start a download and check that the downloads panel opens correctly according
 * to the download parameter, openDownloadsListOnStart
 * @param {boolean} [openDownloadsListOnStart]
 *        true (default) - open downloads panel when download starts
 *        false - no downloads panel; update indicator attention state
 */
async function downloadAndCheckPanel({ openDownloadsListOnStart = true } = {}) {
  info("creating a download and setting it to in progress");
  await task_addDownloads([
    {
      state: DownloadsCommon.DOWNLOAD_DOWNLOADING,
      openDownloadsListOnStart,
    },
  ]);
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  downloads[0].stopped = false;

  // Make sure we remove that download at the end of the test.
  let oldShowEventNotification = DownloadsIndicatorView.showEventNotification;
  registerCleanupFunction(async () => {
    for (let download of downloads) {
      await publicList.remove(download);
    }
    DownloadsIndicatorView.showEventNotification = oldShowEventNotification;
  });

  // Instead of the panel opening, the download notification should be shown.
  let promiseDownloadStartedNotification = new Promise(resolve => {
    DownloadsIndicatorView.showEventNotification = aType => {
      if (aType == "start") {
        resolve();
      }
    };
  });

  DownloadsCommon.getData(window)._notifyDownloadEvent("start", {
    openDownloadsListOnStart,
  });
  is(
    DownloadsPanel.isPanelShowing,
    false,
    "Panel state should indicate it is not preparing to be opened"
  );

  info("waiting for download to start");
  await promiseDownloadStartedNotification;

  DownloadsIndicatorView.showEventNotification = oldShowEventNotification;
  is(DownloadsPanel.panel.state, "closed", "Panel should be closed");
}

function clickCheckbox(checkbox) {
  // Clicking a checkbox toggles its checkedness first.
  if (checkbox.getAttribute("checked") == "true") {
    checkbox.removeAttribute("checked");
  } else {
    checkbox.setAttribute("checked", "true");
  }
  // Then it runs the command and closes the popup.
  checkbox.doCommand();
  checkbox.parentElement.hidePopup();
}

/**
 * Make sure the downloads panel opens automatically with a new download.
 */
add_task(async function test_downloads_panel_opens() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
    ],
  });
  await checkPanelOpens();
});

add_task(async function test_customizemode_doesnt_wreck_things() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
    ],
  });

  // Enter customize mode:
  let customizationReadyPromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "customizationready"
  );
  gCustomizeMode.enter();
  await customizationReadyPromise;

  info("try to open the panel (will not work, in customize mode)");
  let promise = promisePanelOpened();
  DownloadsCommon.getData(window)._notifyDownloadEvent("start");
  await TestUtils.waitForCondition(
    () => DownloadsPanel._state == DownloadsPanel.kStateHidden,
    "Should try to show but stop short and hide the panel"
  );
  is(
    DownloadsPanel._state,
    DownloadsPanel.kStateHidden,
    "Should not start opening the panel."
  );

  let afterCustomizationPromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "aftercustomization"
  );
  gCustomizeMode.exit();
  await afterCustomizationPromise;

  DownloadsCommon.getData(window)._notifyDownloadEvent("start");
  is(
    DownloadsPanel.isPanelShowing,
    true,
    "Panel state should indicate a preparation to be opened"
  );
  await promise;

  is(DownloadsPanel.panel.state, "open", "Panel should be opened");

  DownloadsPanel.hidePanel();
});

/**
 * Make sure the downloads panel _does not_ open automatically if we set the
 * pref telling it not to do that.
 */
add_task(async function test_downloads_panel_opening_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.alwaysOpenPanel", false],
    ],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await downloadAndCheckPanel();
  await SpecialPowers.popPrefEnv();
});

/**
 * Make sure the downloads panel _does not_ open automatically if we pass the
 * parameter telling it not to do that to the download constructor.
 */
add_task(async function test_downloads_openDownloadsListOnStart_param() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.alwaysOpenPanel", true],
    ],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await downloadAndCheckPanel({ openDownloadsListOnStart: false });
  await SpecialPowers.popPrefEnv();
});

/**
 * Make sure the downloads panel _does not_ open automatically when an
 * extension calls the browser.downloads.download API method while it is
 * not handling user input, but that we do open it automatically when
 * the same WebExtensions API is called while handling user input
 * (See Bug 1759231)
 */
add_task(async function test_downloads_panel_on_webext_download_api() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.alwaysOpenPanel", true],
    ],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["downloads"],
    },
    background() {
      async function startDownload(downloadOptions) {
        /* globals browser */
        const downloadId = await browser.downloads.download(downloadOptions);
        const downloadDone = new Promise(resolve => {
          browser.downloads.onChanged.addListener(function listener(delta) {
            browser.test.log(`downloads.onChanged = ${JSON.stringify(delta)}`);
            if (
              delta.id == downloadId &&
              delta.state?.current !== "in_progress"
            ) {
              browser.downloads.onChanged.removeListener(listener);
              resolve();
            }
          });
        });

        browser.test.sendMessage("start-download:done");
        await downloadDone;
        await browser.downloads.removeFile(downloadId);
        browser.test.sendMessage("removed-download-file");
      }

      browser.test.onMessage.addListener(
        (msg, { withHandlingUserInput, downloadOptions }) => {
          if (msg !== "start-download") {
            browser.test.fail(`Got unexpected test message: ${msg}`);
            return;
          }

          if (withHandlingUserInput) {
            browser.test.withHandlingUserInput(() =>
              startDownload(downloadOptions)
            );
          } else {
            startDownload(downloadOptions);
          }
        }
      );
    },
  });

  await extension.startup();

  startServer();

  async function testExtensionDownloadCall({ withHandlingUserInput }) {
    mustInterruptResponses();
    let rnd = Math.random();
    let url = httpUrl(`interruptible.txt?q=${rnd}`);

    extension.sendMessage("start-download", {
      withHandlingUserInput,
      downloadOptions: { url },
    });
    await extension.awaitMessage("start-download:done");

    let publicList = await Downloads.getList(Downloads.PUBLIC);
    let downloads = await publicList.getAll();

    let download = downloads.find(d => d.source.url === url);
    is(download.source.url, url, "download has the expected url");
    is(
      download.openDownloadsListOnStart,
      withHandlingUserInput,
      `download panel should ${withHandlingUserInput ? "open" : "stay closed"}`
    );

    continueResponses();
    await extension.awaitMessage("removed-download-file");
  }

  info(
    "Test extension downloads.download API method call without handling user input"
  );
  await testExtensionDownloadCall({ withHandlingUserInput: true });

  info(
    "Test extension downloads.download API method call while handling user input"
  );
  await testExtensionDownloadCall({ withHandlingUserInput: false });

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

/**
 * Make sure the downloads panel opens automatically with new download, only if
 * no other downloads are in progress.
 */
add_task(async function test_downloads_panel_remains_closed() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
    ],
  });
  await task_addDownloads([
    { state: DownloadsCommon.DOWNLOAD_DOWNLOADING },
    { state: DownloadsCommon.DOWNLOAD_DOWNLOADING },
  ]);

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();

  info("setting 2 downloads to be in progress");
  downloads[0].stopped = false;
  downloads[1].stopped = false;

  let oldShowEventNotification = DownloadsIndicatorView.showEventNotification;

  registerCleanupFunction(async () => {
    // Remove all downloads created during the test.
    for (let download of downloads) {
      await publicList.remove(download);
    }
    DownloadsIndicatorView.showEventNotification = oldShowEventNotification;
  });

  let promiseDownloadStartedNotification = new Promise(resolve => {
    // Instead of downloads panel opening, download notification should be shown.
    DownloadsIndicatorView.showEventNotification = aType => {
      if (aType == "start") {
        DownloadsIndicatorView.showEventNotification = oldShowEventNotification;
        resolve();
      }
    };
  });

  DownloadsCommon.getData(window)._notifyDownloadEvent("start");

  is(
    DownloadsPanel.isPanelShowing,
    false,
    "Panel state should NOT indicate a preparation to be opened"
  );

  await promiseDownloadStartedNotification;

  is(DownloadsPanel.panel.state, "closed", "Panel should be closed");

  for (let download of downloads) {
    await publicList.remove(download);
  }
  is((await publicList.getAll()).length, 0, "Should have no downloads left.");
});

/**
 * Make sure the downloads panel doesn't open if the window isn't in the
 * foreground.
 */
add_task(async function test_downloads_panel_inactive_window() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
    ],
  });

  let oldShowEventNotification = DownloadsIndicatorView.showEventNotification;

  registerCleanupFunction(async () => {
    DownloadsIndicatorView.showEventNotification = oldShowEventNotification;
  });

  let promiseDownloadStartedNotification = new Promise(resolve => {
    // Instead of downloads panel opening, download notification should be shown.
    DownloadsIndicatorView.showEventNotification = aType => {
      if (aType == "start") {
        DownloadsIndicatorView.showEventNotification = oldShowEventNotification;
        resolve();
      }
    };
  });

  let testRunnerWindow = Array.from(Services.wm.getEnumerator("")).find(
    someWin => someWin != window
  );

  await SimpleTest.promiseFocus(testRunnerWindow);

  DownloadsCommon.getData(window)._notifyDownloadEvent("start");

  is(
    DownloadsPanel.isPanelShowing,
    false,
    "Panel state should NOT indicate a preparation to be opened"
  );

  await promiseDownloadStartedNotification;
  await SimpleTest.promiseFocus(window);

  is(DownloadsPanel.panel.state, "closed", "Panel should be closed");

  testRunnerWindow = null;
});

/**
 * When right-clicking the downloads toolbar button, there should be a menuitem
 * for toggling alwaysOpenPanel. Check that it works correctly.
 */
add_task(async function test_alwaysOpenPanel_menuitem() {
  const alwaysOpenPanelPref = "browser.download.alwaysOpenPanel";
  let checkbox = document.getElementById(
    "toolbar-context-always-open-downloads-panel"
  );
  let button = document.getElementById("downloads-button");

  Services.prefs.clearUserPref(alwaysOpenPanelPref);
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.autohideButton", false]],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    Services.prefs.clearUserPref(alwaysOpenPanelPref);
  });

  is(button.hidden, false, "Downloads button should not be hidden.");

  info("Check context menu for downloads button.");
  await openContextMenu(button);
  is(checkbox.hidden, false, "Always Open checkbox is visible.");
  is(checkbox.getAttribute("checked"), "true", "Always Open is enabled.");

  info("Disable Always Open via context menu.");
  clickCheckbox(checkbox);
  is(
    Services.prefs.getBoolPref(alwaysOpenPanelPref),
    false,
    "Always Open pref has been set to false."
  );

  await downloadAndCheckPanel();

  await openContextMenu(button);
  is(checkbox.hidden, false, "Always Open checkbox is visible.");
  isnot(checkbox.getAttribute("checked"), "true", "Always Open is disabled.");

  info("Enable Always Open via context menu");
  clickCheckbox(checkbox);
  is(
    Services.prefs.getBoolPref(alwaysOpenPanelPref),
    true,
    "Pref has been set to true"
  );

  await checkPanelOpens();
});
