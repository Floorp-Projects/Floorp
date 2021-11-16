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
});
