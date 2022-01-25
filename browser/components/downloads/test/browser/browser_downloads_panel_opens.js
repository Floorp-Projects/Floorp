/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the downloads panel opens automatically with a new download.
 */
add_task(async function test_downloads_panel_opens() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.improvements_to_download_panel", true]],
  });

  info("waiting for panel to open");
  let promise = promisePanelOpened();
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
      ["browser.download.alwaysOpenPanel", false],
    ],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });

  info("creating a download and setting it to in progress");
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_DOWNLOADING }]);
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

  DownloadsCommon.getData(window)._notifyDownloadEvent("start");
  is(
    DownloadsPanel.isPanelShowing,
    false,
    "Panel state should indicate it is not preparing to be opened"
  );

  info("waiting for download to start");
  await promiseDownloadStartedNotification;

  is(DownloadsPanel.panel.state, "closed", "Panel should be closed");
  await SpecialPowers.popPrefEnv();
});

/**
 * Make sure the downloads panel opens automatically with new download, only if  no other downloads are in progress.
 */
add_task(async function test_downloads_panel_remains_closed() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.improvements_to_download_panel", true]],
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
    set: [["browser.download.improvements_to_download_panel", true]],
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
