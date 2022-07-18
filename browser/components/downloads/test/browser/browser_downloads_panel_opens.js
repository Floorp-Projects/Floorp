/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { MockFilePicker } = SpecialPowers;
MockFilePicker.init(window);
registerCleanupFunction(() => MockFilePicker.cleanup());

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
 * Test that the downloads panel correctly opens or doesn't open based on
 * whether the download triggered a dialog already. If askWhereToSave is true,
 * we should get a file picker dialog. If preferredAction is alwaysAsk, we
 * should get an unknown content type dialog. If neither of those is true, we
 * should get no dialog at all, and expect the downloads panel to open.
 * @param {boolean} [expectPanelToOpen] true - fail if panel doesn't open
 *                                      false (default) - fail if it opens
 * @param {number}  [preferredAction]   Default download action:
 *                                      0 (default) - save download to disk
 *                                      1 - open UCT dialog first
 * @param {boolean} [askWhereToSave]    true - open file picker dialog
 *                                      false (default) - use download dir
 */
async function testDownloadsPanelAfterDialog({
  expectPanelToOpen = false,
  preferredAction,
  askWhereToSave = false,
} = {}) {
  const { saveToDisk, alwaysAsk } = Ci.nsIHandlerInfo;
  if (![saveToDisk, alwaysAsk].includes(preferredAction)) {
    preferredAction = saveToDisk;
  }
  const openUCT = preferredAction === alwaysAsk;
  const TEST_PATH = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  );
  const MimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  const HandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  let publicList = await Downloads.getList(Downloads.PUBLIC);

  for (let download of await publicList.getAll()) {
    await publicList.remove(download);
  }

  // We need to test the changes from bug 1739348, where the helper app service
  // sets a flag based on whether a file picker dialog was opened, and this flag
  // determines whether the downloads panel will be opened as the download
  // starts. We need to actually hit "Save" for the download to start, but we
  // can't interact with the real file picker dialog. So this temporarily
  // replaces it with a barebones component that plugs into the helper app
  // service and tells it to start saving the file to the default path.
  if (askWhereToSave) {
    MockFilePicker.returnValue = MockFilePicker.returnOK;
    MockFilePicker.showCallback = function(fp) {
      // Get the default location from the helper app service.
      let testFile = MockFilePicker.displayDirectory.clone();
      testFile.append(fp.defaultString);
      info("File picker download path: " + testFile.path);
      MockFilePicker.setFiles([testFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
      MockFilePicker.showCallback = null;
      // Confirm that saving should proceed. The helper app service uses this
      // value to determine whether to invoke launcher.saveDestinationAvailable
      return MockFilePicker.returnOK;
    };
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.useDownloadDir", !askWhereToSave],
      ["browser.download.always_ask_before_handling_new_types", openUCT],
      ["browser.download.improvements_to_download_panel", true],
      ["security.dialog_enable_delay", 0],
    ],
  });

  // Configure the handler for the file according to parameters.
  let mimeInfo = MimeSvc.getFromTypeAndExtension("text/plain", "txt");
  let existed = HandlerSvc.exists(mimeInfo);
  mimeInfo.alwaysAskBeforeHandling = openUCT;
  mimeInfo.preferredAction = preferredAction;
  HandlerSvc.store(mimeInfo);
  registerCleanupFunction(async () => {
    // Reset the handler to its original state.
    if (existed) {
      HandlerSvc.store(mimeInfo);
    } else {
      HandlerSvc.remove(mimeInfo);
    }
    await publicList.removeFinished();
    BrowserTestUtils.removeTab(loadingTab);
  });

  let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let downloadFinishedPromise = new Promise(resolve => {
    publicList.addView({
      onDownloadChanged(download) {
        info("Download changed!");
        if (download.succeeded || download.error) {
          info("Download succeeded or failed.");
          publicList.removeView(this);
          resolve(download);
        }
      },
    });
  });
  let panelOpenedPromise = expectPanelToOpen ? promisePanelOpened() : null;

  // Open the tab that will trigger the download.
  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "foo.txt",
    waitForLoad: false,
    waitForStateStop: true,
  });

  // Wait for a UCT dialog if the handler was set up to open one.
  if (openUCT) {
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );
    let doc = dialogWindow.document;
    let dialog = doc.getElementById("unknownContentType");
    let radio = doc.getElementById("save");
    let button = dialog.getButton("accept");

    await TestUtils.waitForCondition(
      () => !button.disabled,
      "Waiting for the UCT dialog's Accept button to be enabled."
    );
    ok(!radio.hidden, "The Save option should be visible");
    // Make sure we aren't opening the file.
    radio.click();
    ok(radio.selected, "The Save option should be selected");
    button.disabled = false;
    dialog.acceptDialog();
  }

  info("Waiting for download to finish.");
  let download = await downloadFinishedPromise;
  ok(!download.error, "There should be no error.");
  is(
    DownloadsPanel.isPanelShowing,
    expectPanelToOpen,
    `Panel should${expectPanelToOpen ? " " : " not "}be showing.`
  );
  if (DownloadsPanel.isPanelShowing) {
    await panelOpenedPromise;
    let hiddenPromise = BrowserTestUtils.waitForPopupEvent(
      DownloadsPanel.panel,
      "hidden"
    );
    DownloadsPanel.hidePanel();
    await hiddenPromise;
  }
  if (download?.target.exists) {
    try {
      info("Removing test file: " + download.target.path);
      if (Services.appinfo.OS === "WINNT") {
        await IOUtils.setPermissions(download.target.path, 0o600);
      }
      await IOUtils.remove(download.target.path);
    } catch (ex) {
      /* ignore */
    }
  }
  for (let dl of await publicList.getAll()) {
    await publicList.remove(dl);
  }
  BrowserTestUtils.removeTab(loadingTab);
}

/**
 * Make sure the downloads panel opens automatically with a new download.
 */
add_task(async function test_downloads_panel_opens() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.alwaysOpenPanel", true],
    ],
  });
  await checkPanelOpens();
});

add_task(async function test_customizemode_doesnt_wreck_things() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.alwaysOpenPanel", true],
    ],
  });

  // Enter customize mode:
  let customizationReadyPromise = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "customizationready"
  );
  gCustomizeMode.enter();
  await customizationReadyPromise;

  info("Try to open the panel (will not work, in customize mode)");
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

  // Avoid a failure on Linux where the window isn't active for some reason,
  // which prevents the window's downloads panel from opening.
  if (Services.focus.activeWindow != window) {
    info("Main window is not active, trying to focus.");
    await SimpleTest.promiseFocus(window);
    is(Services.focus.activeWindow, window, "Main window should be active.");
  }
  DownloadsCommon.getData(window)._notifyDownloadEvent("start");
  await TestUtils.waitForCondition(
    () => DownloadsPanel.isPanelShowing,
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

/**
 * Verify that the downloads panel opens if the download did not open a file
 * picker or UCT dialog
 */
add_task(async function test_downloads_panel_after_no_dialogs() {
  await testDownloadsPanelAfterDialog({ expectPanelToOpen: true });
  ok(true, "Downloads panel opened because no dialogs were opened.");
});

/**
 * Verify that the downloads panel doesn't open if the download opened an
 * unknown content type dialog (e.g. action = always ask)
 */
add_task(async function test_downloads_panel_after_UCT_dialog() {
  await testDownloadsPanelAfterDialog({
    expectPanelToOpen: false,
    preferredAction: Ci.nsIHandlerInfo.alwaysAsk,
  });
  ok(true, "Downloads panel suppressed after UCT dialog.");
});

/**
 * Verify that the downloads panel doesn't open if the download opened a file
 * picker dialog (e.g. useDownloadDir = false)
 */
add_task(async function test_downloads_panel_after_file_picker_dialog() {
  await testDownloadsPanelAfterDialog({
    expectPanelToOpen: false,
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    askWhereToSave: true,
  });
  ok(true, "Downloads panel suppressed after file picker dialog.");
});

/**
 * Verify that the downloads panel doesn't open if the download opened both
 * dialogs (e.g. default action = always ask AND useDownloadDir = false)
 */
add_task(async function test_downloads_panel_after_both_dialogs() {
  await testDownloadsPanelAfterDialog({
    expectPanelToOpen: false,
    preferredAction: Ci.nsIHandlerInfo.alwaysAsk,
    askWhereToSave: true,
  });
  ok(true, "Downloads panel suppressed after UCT and file picker dialogs.");
});
