"use strict";

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const kTestURI = kTestPath + "file_data_text_csv.html";

function addWindowListener(aURL, aCallback) {
  return new Promise(resolve => {
    Services.wm.addListener({
      onOpenWindow(aXULWindow) {
        info("window opened, waiting for focus");
        Services.wm.removeListener(this);
        var domwindow = aXULWindow.docShell.domWindow;
        waitForFocus(function() {
          is(
            domwindow.document.location.href,
            aURL,
            "should have seen the right window open"
          );
          resolve(domwindow);
        }, domwindow);
      },
      onCloseWindow(aXULWindow) {},
    });
  });
}

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

// Note: remove this test once browser.download.improvements_to_download_panel is removed.
add_task(async function test_with_downloads_improvement_pref_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.data_uri.block_toplevel_data_uri_navigations", true],
      ["browser.download.improvements_to_download_panel", false],
    ],
  });

  let windowPromise = addWindowListener(
    "chrome://mozapps/content/downloads/unknownContentType.xhtml"
  );
  BrowserTestUtils.loadURI(gBrowser, kTestURI);
  let win = await windowPromise;

  let expectedValue = "text/csv;foo,bar,foobar";
  is(
    win.document.getElementById("location").value,
    expectedValue,
    "file name of download should match"
  );
  let mainWindowActivated = BrowserTestUtils.waitForEvent(window, "activate");
  await BrowserTestUtils.closeWindow(win);
  await mainWindowActivated;
});

add_task(async function test_with_downloads_improvement_pref_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.data_uri.block_toplevel_data_uri_navigations", true],
      ["browser.download.improvements_to_download_panel", true],
    ],
  });
  let downloadsPanelPromise = promisePanelOpened();
  let downloadsPromise = Downloads.getList(Downloads.PUBLIC);
  let expectedValue = "text/csv;foo,bar,foobar";

  info("Loading URI for pref enabled");
  BrowserTestUtils.loadURI(gBrowser, kTestURI);
  info("Waiting for downloads panel to open");
  await downloadsPanelPromise;
  info("Getting downloads info after opening downloads panel");
  let downloadList = await downloadsPromise;

  is(DownloadsPanel.isPanelShowing, true, "DownloadsPanel should be open.");
  is(
    downloadList._downloads.length,
    1,
    "File should be successfully downloaded."
  );

  let [download] = downloadList._downloads;
  is(download.contentType, "text/csv", "File contentType should be correct.");
  is(
    download.source.url,
    `data:${expectedValue}`,
    "File name should be correct."
  );

  info("Cleaning up downloads");
  try {
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(download.target.path, 0o600);
    }
    await IOUtils.remove(download.target.path);
  } catch (ex) {
    info("The file " + download.target.path + " is not removed, " + ex);
  }

  await downloadList.remove(download);
  await download.finalize();
});
