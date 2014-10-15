/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated download components tests.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
const nsIDM = Ci.nsIDownloadManager;

let gTestTargetFile = FileUtils.getFile("TmpD", ["dm-ui-test.file"]);
gTestTargetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
registerCleanupFunction(function () {
  gTestTargetFile.remove(false);
});

////////////////////////////////////////////////////////////////////////////////
//// Asynchronous support subroutines

function promiseFocus()
{
  let deferred = Promise.defer();
  waitForFocus(deferred.resolve);
  return deferred.promise;
}

function promisePanelOpened()
{
  let deferred = Promise.defer();

  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return deferred.resolve();
  }

  // Hook to wait until the panel is shown.
  let originalOnPopupShown = DownloadsPanel.onPopupShown;
  DownloadsPanel.onPopupShown = function () {
    DownloadsPanel.onPopupShown = originalOnPopupShown;
    originalOnPopupShown.apply(this, arguments);

    // Defer to the next tick of the event loop so that we don't continue
    // processing during the DOM event handler itself.
    setTimeout(deferred.resolve, 0);
  };

  return deferred.promise;
}

function task_resetState()
{
  // Remove all downloads.
  let publicList = yield Downloads.getList(Downloads.PUBLIC);
  let downloads = yield publicList.getAll();
  for (let download of downloads) {
    publicList.remove(download);
    yield download.finalize(true);
  }

  DownloadsPanel.hidePanel();

  yield promiseFocus();
}

function task_addDownloads(aItems)
{
  let startTimeMs = Date.now();

  let publicList = yield Downloads.getList(Downloads.PUBLIC);
  for (let item of aItems) {
    publicList.add(yield Downloads.createDownload({
      source: "http://www.example.com/test-download.txt",
      target: gTestTargetFile,
      succeeded: item.state == nsIDM.DOWNLOAD_FINISHED,
      canceled: item.state == nsIDM.DOWNLOAD_CANCELED ||
                item.state == nsIDM.DOWNLOAD_PAUSED,
      error: item.state == nsIDM.DOWNLOAD_FAILED ? new Error("Failed.") : null,
      hasPartialData: item.state == nsIDM.DOWNLOAD_PAUSED,
      startTime: new Date(startTimeMs++),
    }));
  }
}

function task_openPanel()
{
  yield promiseFocus();

  let promise = promisePanelOpened();
  DownloadsPanel.showPanel();
  yield promise;
}
