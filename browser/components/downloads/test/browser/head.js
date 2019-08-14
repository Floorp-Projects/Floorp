/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated download components tests.
 */

// Globals

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
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "HttpServer",
  "resource://testing-common/httpd.js"
);

var gTestTargetFile = FileUtils.getFile("TmpD", ["dm-ui-test.file"]);
gTestTargetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

// The file may have been already deleted when removing a paused download.
registerCleanupFunction(() =>
  OS.File.remove(gTestTargetFile.path, { ignoreAbsent: true })
);

// Asynchronous support subroutines

function promiseFocus() {
  return new Promise(resolve => {
    waitForFocus(resolve);
  });
}

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    // Hook to wait until the panel is shown.
    let originalOnPopupShown = DownloadsPanel.onPopupShown;
    DownloadsPanel.onPopupShown = function() {
      DownloadsPanel.onPopupShown = originalOnPopupShown;
      originalOnPopupShown.apply(this, arguments);

      // Defer to the next tick of the event loop so that we don't continue
      // processing during the DOM event handler itself.
      setTimeout(resolve, 0);
    };
  });
}

async function task_resetState() {
  // Remove all downloads.
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    publicList.remove(download);
    await download.finalize(true);
  }

  DownloadsPanel.hidePanel();

  await promiseFocus();
}

async function task_addDownloads(aItems) {
  let startTimeMs = Date.now();

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  for (let item of aItems) {
    let download = {
      source: {
        url: "http://www.example.com/test-download.txt",
      },
      target: {
        path: gTestTargetFile.path,
      },
      succeeded: item.state == DownloadsCommon.DOWNLOAD_FINISHED,
      canceled:
        item.state == DownloadsCommon.DOWNLOAD_CANCELED ||
        item.state == DownloadsCommon.DOWNLOAD_PAUSED,
      error:
        item.state == DownloadsCommon.DOWNLOAD_FAILED
          ? new Error("Failed.")
          : null,
      hasPartialData: item.state == DownloadsCommon.DOWNLOAD_PAUSED,
      hasBlockedData: item.hasBlockedData || false,
      startTime: new Date(startTimeMs++),
    };
    // `"errorObj" in download` must be false when there's no error.
    if (item.errorObj) {
      download.errorObj = item.errorObj;
    }
    await publicList.add(await Downloads.createDownload(download));
  }
}

async function task_openPanel() {
  await promiseFocus();

  let promise = promisePanelOpened();
  DownloadsPanel.showPanel();
  await promise;
}

async function setDownloadDir() {
  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tmpDir.append("testsavedir");
  if (!tmpDir.exists()) {
    tmpDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    registerCleanupFunction(function() {
      try {
        tmpDir.remove(true);
      } catch (e) {
        // On Windows debug build this may fail.
      }
    });
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.folderList", 2],
      ["browser.download.dir", tmpDir.path],
    ],
  });
}

let gHttpServer = null;
function startServer() {
  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  registerCleanupFunction(async function() {
    await new Promise(function(resolve) {
      gHttpServer.stop(resolve);
    });
  });

  gHttpServer.registerPathHandler("/file1.txt", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write("file1");
    response.processAsync();
    response.finish();
  });
  gHttpServer.registerPathHandler("/file2.txt", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write("file2");
    response.processAsync();
    response.finish();
  });
  gHttpServer.registerPathHandler("/file3.txt", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write("file3");
    response.processAsync();
    response.finish();
  });
}

function httpUrl(aFileName) {
  return (
    "http://localhost:" + gHttpServer.identity.primaryPort + "/" + aFileName
  );
}

function openLibrary(aLeftPaneRoot) {
  let library = window.openDialog(
    "chrome://browser/content/places/places.xul",
    "",
    "chrome,toolbar=yes,dialog=no,resizable",
    aLeftPaneRoot
  );

  return new Promise(resolve => {
    waitForFocus(resolve, library);
  });
}

/**
 * Waits for a given button to become visible.
 */
function promiseButtonShown(id) {
  let dwu = window.windowUtils;
  return BrowserTestUtils.waitForCondition(() => {
    let target = document.getElementById(id);
    let bounds = dwu.getBoundsWithoutFlushing(target);
    return bounds.width > 0 && bounds.height > 0;
  }, `Waiting for button ${id} to have non-0 size`);
}

async function simulateDropAndCheck(win, dropTarget, urls) {
  let dragData = [[{ type: "text/plain", data: urls.join("\n") }]];
  let list = await Downloads.getList(Downloads.ALL);

  let added = new Set();
  let succeeded = new Set();
  await new Promise(resolve => {
    let view = {
      onDownloadAdded(download) {
        added.add(download.source.url);
      },
      onDownloadChanged(download) {
        if (!added.has(download.source.url)) {
          return;
        }
        if (!download.succeeded) {
          return;
        }
        succeeded.add(download.source.url);
        if (succeeded.size == urls.length) {
          list.removeView(view).then(resolve);
        }
      },
    };
    list.addView(view).then(function() {
      EventUtils.synthesizeDrop(dropTarget, dropTarget, dragData, "link", win);
    });
  });

  for (let url of urls) {
    ok(added.has(url), url + " is added to download");
  }
}
