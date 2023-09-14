"use strict";

ChromeUtils.defineESModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});
// Create a uri for an https site
const testPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_URI = testPath + "file_slow_download.html";
const EXPECTED_DOWNLOAD_URL =
  "example.com/browser/dom/security/test/https-first/file_slow_download.sjs";

// Since the server send the complete download file after 3 seconds we need an longer timeout
requestLongerTimeout(4);

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

/**
 * Waits for a download to finish, in case it has not finished already.
 *
 * @param aDownload
 *        The Download object to wait upon.
 *
 * @return {Promise}
 * @resolves When the download has finished successfully.
 * @rejects JavaScript exception if the download failed.
 */
function promiseDownloadStopped(aDownload) {
  if (!aDownload.stopped) {
    // The download is in progress, wait for the current attempt to finish and
    // report any errors that may occur.
    return aDownload.start();
  }

  if (aDownload.succeeded) {
    return Promise.resolve();
  }

  // The download failed or was canceled.
  return Promise.reject(aDownload.error || new Error("Download canceled."));
}

// Verifys that no background request was send
let requestCounter = 0;
function examiner() {
  SpecialPowers.addObserver(this, "specialpowers-http-notify-request");
}

examiner.prototype = {
  observe(subject, topic, data) {
    if (topic !== "specialpowers-http-notify-request") {
      return;
    }
    // On Android we have other requests appear here as well. Let's make
    // sure we only evaluate requests triggered by the test.
    if (
      !data.startsWith("http://example.com") &&
      !data.startsWith("https://example.com")
    ) {
      return;
    }
    ++requestCounter;
    if (requestCounter == 1) {
      is(data, TEST_URI, "Download start page is https");
      return;
    }
    if (requestCounter == 2) {
      // The specialpowers-http-notify-request fires before the internal redirect( /upgrade) to
      // https happens.
      is(
        data,
        "http://" + EXPECTED_DOWNLOAD_URL,
        "First download request is http (internal)"
      );
      return;
    }
    if (requestCounter == 3) {
      is(
        data,
        "https://" + EXPECTED_DOWNLOAD_URL,
        "Download got upgraded to https"
      );
      return;
    }
    ok(false, "we should never get here, but just in case");
  },
  remove() {
    SpecialPowers.removeObserver(this, "specialpowers-http-notify-request");
  },
};

// Test description:
// 1. Open https://example.com
// 2. Start download - location of download is http
// 3. https-first upgrades to https
// 4. Server send first part of download and after 3 seconds the rest
// 5. Complete download of text file
add_task(async function test_slow_download() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  // remove all previous downloads
  let downloadsList = await Downloads.getList(Downloads.PUBLIC);
  await downloadsList.removeFinished();

  // add observer to ensure that the background request gets canceled for the upgraded Download
  this.examiner = new examiner();

  let downloadsPanelPromise = promisePanelOpened();
  let downloadsPromise = Downloads.getList(Downloads.PUBLIC);
  BrowserTestUtils.startLoadingURIString(gBrowser, TEST_URI);
  // wait for downloadsPanel to open before continuing with test
  await downloadsPanelPromise;
  let downloadList = await downloadsPromise;
  is(DownloadsPanel.isPanelShowing, true, "DownloadsPanel should be open.");
  is(downloadList._downloads.length, 1, "File should be downloaded.");
  let [download] = downloadList._downloads;
  // wait for download to finish (with success or error)
  await promiseDownloadStopped(download);
  is(download.contentType, "text/plain", "File contentType should be correct.");
  // ensure https-first did upgrade the scheme.
  is(
    download.source.url,
    "https://" + EXPECTED_DOWNLOAD_URL,
    "Scheme should be https."
  );
  // ensure that no background request was send
  is(
    requestCounter,
    3,
    "three requests total (download page, download http, download https/ upgraded)"
  );
  // ensure that downloaded is complete
  is(download.target.size, 25, "Download size is correct");
  //clean up
  this.examiner.remove();
  info("cleaning up downloads");
  try {
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(download.target.path, 0o600);
    }
    await IOUtils.remove(download.target.path);
  } catch (error) {
    info("The file " + download.target.path + " is not removed, " + error);
  }

  await downloadList.remove(download);
  await download.finalize();
});
