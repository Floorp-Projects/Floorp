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
ChromeUtils.defineESModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  this,
  "HttpServer",
  "resource://testing-common/httpd.js"
);

let gTestTargetFile = new FileUtils.File(
  PathUtils.join(
    Services.dirsvc.get("TmpD", Ci.nsIFile).path,
    "dm-ui-test.file"
  )
);

gTestTargetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
Services.prefs.setIntPref("security.dialog_enable_delay", 0);

// The file may have been already deleted when removing a paused download.
// Also clear security.dialog_enable_delay pref.
registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("security.dialog_enable_delay");

  if (await IOUtils.exists(gTestTargetFile.path)) {
    info("removing " + gTestTargetFile.path);
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(gTestTargetFile.path, 0o600);
    }
    await IOUtils.remove(gTestTargetFile.path);
  }
});

const DATA_PDF = atob(
  "JVBERi0xLjANCjEgMCBvYmo8PC9UeXBlL0NhdGFsb2cvUGFnZXMgMiAwIFI+PmVuZG9iaiAyIDAgb2JqPDwvVHlwZS9QYWdlcy9LaWRzWzMgMCBSXS9Db3VudCAxPj5lbmRvYmogMyAwIG9iajw8L1R5cGUvUGFnZS9NZWRpYUJveFswIDAgMyAzXT4+ZW5kb2JqDQp4cmVmDQowIDQNCjAwMDAwMDAwMDAgNjU1MzUgZg0KMDAwMDAwMDAxMCAwMDAwMCBuDQowMDAwMDAwMDUzIDAwMDAwIG4NCjAwMDAwMDAxMDIgMDAwMDAgbg0KdHJhaWxlcjw8L1NpemUgNC9Sb290IDEgMCBSPj4NCnN0YXJ0eHJlZg0KMTQ5DQolRU9G"
);

const TEST_DATA_SHORT = "This test string is downloaded.";

/**
 * This is an internal reference that should not be used directly by tests.
 */
var _gDeferResponses = PromiseUtils.defer();

/**
 * Ensures that all the interruptible requests started after this function is
 * called won't complete until the continueResponses function is called.
 *
 * Normally, the internal HTTP server returns all the available data as soon as
 * a request is received.  In order for some requests to be served one part at a
 * time, special interruptible handlers are registered on the HTTP server.  This
 * allows testing events or actions that need to happen in the middle of a
 * download.
 *
 * For example, the handler accessible at the httpUri("interruptible.txt")
 * address returns the TEST_DATA_SHORT text, then it may block until the
 * continueResponses method is called.  At this point, the handler sends the
 * TEST_DATA_SHORT text again to complete the response.
 *
 * If an interruptible request is started before the function is called, it may
 * or may not be blocked depending on the actual sequence of events.
 */
function mustInterruptResponses() {
  // If there are pending blocked requests, allow them to complete.  This is
  // done to prevent requests from being blocked forever, but should not affect
  // the test logic, since previously started requests should not be monitored
  // on the client side anymore.
  _gDeferResponses.resolve();

  info("Interruptible responses will be blocked midway.");
  _gDeferResponses = PromiseUtils.defer();
}

/**
 * Allows all the current and future interruptible requests to complete.
 */
function continueResponses() {
  info("Interruptible responses are now allowed to continue.");
  _gDeferResponses.resolve();
}

/**
 * Creates a download, which could be interrupted in the middle of it's progress.
 */
function promiseInterruptibleDownload(extension = ".txt") {
  let interruptibleFile = FileUtils.getFile("TmpD", [
    `interruptible${extension}`,
  ]);
  interruptibleFile.createUnique(
    Ci.nsIFile.NORMAL_FILE_TYPE,
    FileUtils.PERMS_FILE
  );

  registerCleanupFunction(async () => {
    if (await IOUtils.exists(interruptibleFile.path)) {
      if (Services.appinfo.OS === "WINNT") {
        // We need to make the file writable to delete it on Windows.
        await IOUtils.setPermissions(interruptibleFile.path, 0o600);
      }
      await IOUtils.remove(interruptibleFile.path);
    }
  });

  return Downloads.createDownload({
    source: httpUrl("interruptible.txt"),
    target: { path: interruptibleFile.path },
  });
}

// Asynchronous support subroutines

async function createDownloadedFile(pathname, contents) {
  let file = new FileUtils.File(pathname);
  if (file.exists()) {
    info(`File at ${pathname} already exists`);
  }
  // No post-test cleanup necessary; tmp downloads directory is already removed after each test
  await IOUtils.writeUTF8(pathname, contents);
  ok(file.exists(), `Created ${pathname}`);
  return file;
}

async function openContextMenu(itemElement, win = window) {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    itemElement.ownerDocument,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    itemElement,
    {
      type: "contextmenu",
      button: 2,
    },
    win
  );
  let { target } = await popupShownPromise;
  return target;
}

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
    await publicList.remove(download);
    if (await IOUtils.exists(download.target.path)) {
      await download.finalize(true);
      info("removing " + download.target.path);
      if (Services.appinfo.OS === "WINNT") {
        // We need to make the file writable to delete it on Windows.
        await IOUtils.setPermissions(download.target.path, 0o600);
      }
      await IOUtils.remove(download.target.path);
    }
  }

  DownloadsPanel.hidePanel();

  await promiseFocus();
}

async function task_addDownloads(aItems) {
  let startTimeMs = Date.now();

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  for (let item of aItems) {
    let source = {
      url: "http://www.example.com/test-download.txt",
      ...item.source,
    };
    let target =
      item.target instanceof Ci.nsIFile
        ? item.target
        : {
            path: gTestTargetFile.path,
            ...item.target,
          };

    let download = {
      source,
      target,
      succeeded: item.state == DownloadsCommon.DOWNLOAD_FINISHED,
      canceled:
        item.state == DownloadsCommon.DOWNLOAD_CANCELED ||
        item.state == DownloadsCommon.DOWNLOAD_PAUSED,
      deleted: item.deleted ?? false,
      error:
        item.state == DownloadsCommon.DOWNLOAD_FAILED
          ? new Error("Failed.")
          : null,
      hasPartialData: item.state == DownloadsCommon.DOWNLOAD_PAUSED,
      hasBlockedData: item.hasBlockedData || false,
      openDownloadsListOnStart: item.openDownloadsListOnStart ?? true,
      contentType: item.contentType,
      startTime: new Date(startTimeMs++),
    };
    // `"errorObj" in download` must be false when there's no error.
    if (item.errorObj) {
      download.errorObj = item.errorObj;
    }
    download = await Downloads.createDownload(download);
    await publicList.add(download);
    await download.refresh();
  }
}

async function task_openPanel() {
  await promiseFocus();

  let promise = promisePanelOpened();
  DownloadsPanel.showPanel();
  await promise;
}

async function setDownloadDir() {
  let tmpDir = PathUtils.join(
    PathUtils.tempDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function() {
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      Cu.reportError(e);
    }
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
  return tmpDir;
}

let gHttpServer = null;
let gShouldServeInterruptibleFileAsDownload = false;
function startServer() {
  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  registerCleanupFunction(() => {
    return new Promise(resolve => {
      // Ensure all the pending HTTP requests have a chance to finish.
      continueResponses();
      // Stop the HTTP server, calling resolve when it's done.
      gHttpServer.stop(resolve);
    });
  });

  gHttpServer.identity.setPrimary(
    "http",
    "www.example.com",
    gHttpServer.identity.primaryPort
  );

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

  gHttpServer.registerPathHandler("/interruptible.txt", function(
    aRequest,
    aResponse
  ) {
    info("Interruptible request started.");

    // Process the first part of the response.
    aResponse.processAsync();
    aResponse.setHeader("Content-Type", "text/plain", false);
    if (gShouldServeInterruptibleFileAsDownload) {
      aResponse.setHeader("Content-Disposition", "attachment");
    }
    aResponse.setHeader(
      "Content-Length",
      "" + TEST_DATA_SHORT.length * 2,
      false
    );
    aResponse.write(TEST_DATA_SHORT);

    // Wait on the current deferred object, then finish the request.
    _gDeferResponses.promise
      .then(function RIH_onSuccess() {
        aResponse.write(TEST_DATA_SHORT);
        aResponse.finish();
        info("Interruptible request finished.");
      })
      .catch(Cu.reportError);
  });
}

function serveInterruptibleAsDownload() {
  gShouldServeInterruptibleFileAsDownload = true;
  registerCleanupFunction(
    () => (gShouldServeInterruptibleFileAsDownload = false)
  );
}

function httpUrl(aFileName) {
  return (
    "http://localhost:" + gHttpServer.identity.primaryPort + "/" + aFileName
  );
}

function openLibrary(aLeftPaneRoot) {
  let library = window.openDialog(
    "chrome://browser/content/places/places.xhtml",
    "",
    "chrome,toolbar=yes,dialog=no,resizable",
    aLeftPaneRoot
  );

  return new Promise(resolve => {
    waitForFocus(resolve, library);
  });
}

/**
 * Waits for a download to reach its progress, in case it has not
 * reached the expected progress already.
 *
 * @param aDownload
 *        The Download object to wait upon.
 *
 * @return {Promise}
 * @resolves When the download has reached its progress.
 * @rejects Never.
 */
function promiseDownloadHasProgress(aDownload, progress) {
  return new Promise(resolve => {
    // Wait for the download to reach its progress.
    let onchange = function() {
      let downloadInProgress =
        !aDownload.stopped && aDownload.progress == progress;
      let downloadFinished =
        progress == 100 &&
        aDownload.progress == progress &&
        aDownload.succeeded;
      if (downloadInProgress || downloadFinished) {
        info(`Download reached ${progress}%`);
        aDownload.onchange = null;
        resolve();
      }
    };

    // Register for the notification, but also call the function directly in
    // case the download already reached the expected progress.
    aDownload.onchange = onchange;
    onchange();
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
