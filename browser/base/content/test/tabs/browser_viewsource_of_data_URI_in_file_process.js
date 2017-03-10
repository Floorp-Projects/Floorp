/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const DUMMY_FILE = "dummy_page.html";
const DATA_URI = "data:text/html,Hi";
const DATA_URI_SOURCE = "view-source:" + DATA_URI;

// Test for bug 1345807.
add_task(function* () {
  // Open file:// page.
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(DUMMY_FILE);
  const uriString = Services.io.newFileURI(dir).spec;

  yield BrowserTestUtils.withNewTab(uriString, function*(fileBrowser) {
    let filePid = yield ContentTask.spawn(fileBrowser, null, () => {
      return Services.appinfo.processID;
    });

    // Navigate to data URI.
    let promiseLoad = BrowserTestUtils.browserLoaded(fileBrowser, false,
                                                     DATA_URI);
    fileBrowser.loadURI(DATA_URI);
    let href = yield promiseLoad;
    is(href, DATA_URI, "Check data URI loaded.");
    let dataPid = yield ContentTask.spawn(fileBrowser, null, () => {
      return Services.appinfo.processID;
    });
    is(dataPid, filePid, "Check that data URI loaded in file content process.");

    // Make sure we can view-source on the data URI page.
    let promiseTab = BrowserTestUtils.waitForNewTab(gBrowser, DATA_URI_SOURCE);
    BrowserViewSource(fileBrowser);
    let viewSourceTab = yield promiseTab;
    registerCleanupFunction(function* () {
      yield BrowserTestUtils.removeTab(viewSourceTab);
    });
    yield ContentTask.spawn(viewSourceTab.linkedBrowser, DATA_URI_SOURCE, uri => {
      is(content.document.documentURI, uri,
         "Check that a view-source page was loaded.");
    });
  });
});
