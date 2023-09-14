"use strict";

// Create a uri for an http site
//(in that case a site without cert such that https-first isn't upgrading it)
const insecureTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://nocert.example.com"
);
const insecureTestURI = insecureTestPath + "file_download_attribute.html";

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

const CONSOLE_UPGRADE_TRY_MESSAGE = "Upgrading insecure request";
const CONSOLE_ERROR_MESSAGE = "Downgrading to “http” again";
const DOWNLOAD_PAGE_URL =
  "nocert.example.com/browser/dom/security/test/https-first/file_download_attribute.html";
const DOWNLOAD_LINK_URL =
  "nocert.example.com/browser/dom/security/test/https-first/file_download_attribute.sjs";

// Verifys that https-first tried to upgrade the download
// - and that the upgrade attempt failed.
// We will receive 4 messages. Two for upgrading and downgrading
// the download page and another two for upgrading and downgrading
// the download.
let msgCounter = 0;
function shouldConsoleTryUpgradeAndError() {
  // Waits until CONSOLE_ERROR_MESSAGE was logged.
  // Checks if download was tried via http://
  return new Promise((resolve, reject) => {
    function listener(msgObj) {
      let text = msgObj.message;
      // Verify upgrade messages
      if (
        text.includes(CONSOLE_UPGRADE_TRY_MESSAGE) &&
        text.includes("http://")
      ) {
        if (msgCounter == 0) {
          ok(
            text.includes(DOWNLOAD_PAGE_URL),
            "Tries to upgrade nocert example to https"
          );
        } else {
          ok(
            text.includes(DOWNLOAD_LINK_URL),
            "Tries to upgrade download to https"
          );
        }
        msgCounter++;
      }
      // Verify downgrade messages
      if (text.includes(CONSOLE_ERROR_MESSAGE) && msgCounter > 0) {
        if (msgCounter == 1) {
          ok(
            text.includes("https://" + DOWNLOAD_PAGE_URL),
            "Downgrades nocert example to http"
          );
          msgCounter++;
        } else {
          ok(
            text.includes("https://" + DOWNLOAD_LINK_URL),
            "Downgrades download to http"
          );
          Services.console.unregisterListener(listener);
          resolve();
        }
      }
    }
    Services.console.registerListener(listener);
  });
}

// Test https-first download of an html file from an http site.
// Test description:
// 1. https-first tries to upgrade site to https
// 2. upgrade fails because site has no certificate
// 3. https-first downgrades to http and starts download via http
// 4. Successfully completes download
add_task(async function test_with_downloads_pref_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });
  let checkPromise = shouldConsoleTryUpgradeAndError();
  let downloadsPanelPromise = promisePanelOpened();
  let downloadsPromise = Downloads.getList(Downloads.PUBLIC);

  BrowserTestUtils.startLoadingURIString(gBrowser, insecureTestURI);
  // wait for downloadsPanel to open before continuing with test
  await downloadsPanelPromise;
  let downloadList = await downloadsPromise;
  await checkPromise;
  is(DownloadsPanel.isPanelShowing, true, "DownloadsPanel should be open.");
  is(
    downloadList._downloads.length,
    1,
    "File should be successfully downloaded."
  );

  let [download] = downloadList._downloads;
  is(download.contentType, "text/html", "File contentType should be correct.");
  // ensure https-first didn't upgrade the scheme.
  is(
    download.source.url,
    insecureTestPath + "file_download_attribute.sjs",
    "Scheme should be http."
  );

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
