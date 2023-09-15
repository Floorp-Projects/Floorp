"use strict";

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const kTestURI = kTestPath + "file_data_download.html";

function addWindowListener(aURL) {
  return new Promise(resolve => {
    Services.wm.addListener({
      onOpenWindow(aXULWindow) {
        info("window opened, waiting for focus");
        Services.wm.removeListener(this);
        var domwindow = aXULWindow.docShell.domWindow;
        waitForFocus(function () {
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

function waitDelay(delay) {
  return new Promise((resolve, reject) => {
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    window.setTimeout(resolve, delay);
  });
}

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

add_task(async function test_with_downloads_pref_disabled() {
  waitForExplicitFinish();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.data_uri.block_toplevel_data_uri_navigations", true],
      ["browser.download.always_ask_before_handling_new_types", true],
    ],
  });
  let windowPromise = addWindowListener(
    "chrome://mozapps/content/downloads/unknownContentType.xhtml"
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, kTestURI);
  let win = await windowPromise;

  is(
    win.document.getElementById("location").value,
    "data-foo.html",
    "file name of download should match"
  );

  let mainWindowActivated = BrowserTestUtils.waitForEvent(window, "activate");
  await BrowserTestUtils.closeWindow(win);
  await mainWindowActivated;
});

add_task(async function test_with_always_ask_pref_disabled() {
  waitForExplicitFinish();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.data_uri.block_toplevel_data_uri_navigations", true],
      ["browser.download.always_ask_before_handling_new_types", false],
    ],
  });
  let downloadsPanelPromise = promisePanelOpened();
  let downloadsPromise = Downloads.getList(Downloads.PUBLIC);

  BrowserTestUtils.startLoadingURIString(gBrowser, kTestURI);
  // wait until downloadsPanel opens before continuing with test
  await downloadsPanelPromise;
  let downloadList = await downloadsPromise;

  is(DownloadsPanel.isPanelShowing, true, "DownloadsPanel should be open.");
  is(
    downloadList._downloads.length,
    1,
    "File should be successfully downloaded."
  );

  let [download] = downloadList._downloads;
  is(download.contentType, "text/html", "File contentType should be correct.");
  is(
    download.source.url,
    "data:text/html,<body>data download</body>",
    "File name should be correct."
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
