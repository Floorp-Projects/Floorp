/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_FILE = "dummy_page.html";
const TEST_HTTP = "http://example.org/";
const TEST_CROSS_ORIGIN = "http://example.com/";

function CheckBrowserInPid(browser, expectedPid, message) {
  return ContentTask.spawn(browser, { expectedPid, message }, arg => {
    is(Services.appinfo.processID, arg.expectedPid, arg.message);
  });
}

function CheckBrowserNotInPid(browser, unExpectedPid, message) {
  return ContentTask.spawn(browser, { unExpectedPid, message }, arg => {
    isnot(Services.appinfo.processID, arg.unExpectedPid, arg.message);
  });
}

// Test for bug 1343184.
add_task(async function() {
  // Set prefs to ensure file content process, to allow linked web content in
  // file URI process and allow more that one file content process.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separateFileUriProcess", true],
      ["browser.tabs.remote.allowLinkedWebInFileUriProcess", true],
      ["browser.tabs.remote.useHTTPResponseProcessSelection", false],
      ["dom.ipc.processCount.file", 2],
    ],
  });

  // Open file:// page.
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_FILE);
  dir.normalize();
  const uriString = Services.io.newFileURI(dir).spec;
  await BrowserTestUtils.withNewTab(uriString, async function(fileBrowser) {
    // Get the file:// URI pid for comparison later.
    let filePid = await ContentTask.spawn(fileBrowser, null, () => {
      return Services.appinfo.processID;
    });

    // Check that http tab opened from JS in file:// page is in same process.
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      TEST_HTTP,
      true
    );
    await ContentTask.spawn(fileBrowser, TEST_HTTP, uri => {
      content.open(uri, "_blank");
    });
    let httpTab = await promiseTabOpened;
    let httpBrowser = httpTab.linkedBrowser;
    registerCleanupFunction(async function() {
      BrowserTestUtils.removeTab(httpTab);
    });
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that new http tab opened from file loaded in file content process."
    );

    // Check that reload doesn't break the file content process affinity.
    if (httpTab != gBrowser.selectedTab) {
      httpTab = await BrowserTestUtils.switchTab(gBrowser, httpTab);
      httpBrowser = httpTab.linkedBrowser;
    }
    let promiseLoad = BrowserTestUtils.browserLoaded(
      httpBrowser,
      false,
      TEST_HTTP
    );
    document.getElementById("reload-button").doCommand();
    await promiseLoad;
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that http tab still in file content process after reload."
    );

    // Check that same-origin load doesn't break the affinity.
    promiseLoad = BrowserTestUtils.browserLoaded(
      httpBrowser,
      false,
      TEST_HTTP + "foo"
    );
    BrowserTestUtils.loadURI(httpBrowser, TEST_HTTP + "foo");
    await promiseLoad;
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that http tab still in file content process after same origin load."
    );

    // Check that history back doesn't break the affinity.
    let promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_HTTP
    );
    httpBrowser.goBack();
    await promiseLocation;
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that http tab still in file content process after history back."
    );

    // Check that history forward doesn't break the affinity.
    promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_HTTP + "foo"
    );
    promiseLoad = BrowserTestUtils.browserLoaded(
      httpBrowser,
      false,
      TEST_HTTP + "foo"
    );
    httpBrowser.goForward();
    await promiseLocation;
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that http tab still in file content process after history forward."
    );

    // Check that goto history index doesn't break the affinity.
    promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_HTTP
    );
    httpBrowser.gotoIndex(0);
    await promiseLocation;
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that http tab still in file content process after history gotoIndex."
    );

    // Check that file:// URI load doesn't break the affinity.
    promiseLoad = BrowserTestUtils.browserLoaded(httpBrowser, false, uriString);
    BrowserTestUtils.loadURI(httpBrowser, uriString);
    await promiseLoad;
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that http tab still in file content process after file:// load."
    );

    // Check that location change doesn't break the affinity.
    promiseLoad = BrowserTestUtils.browserLoaded(httpBrowser, false, TEST_HTTP);
    await ContentTask.spawn(httpBrowser, TEST_HTTP, uri => {
      content.location = uri;
    });
    await promiseLoad;
    await CheckBrowserInPid(
      httpBrowser,
      filePid,
      "Check that http tab still in file content process after location change."
    );

    // Check that cross-origin load does break the affinity.
    promiseLoad = BrowserTestUtils.browserLoaded(
      httpBrowser,
      false,
      TEST_CROSS_ORIGIN
    );
    BrowserTestUtils.loadURI(httpBrowser, TEST_CROSS_ORIGIN);
    await promiseLoad;
    await CheckBrowserNotInPid(
      httpBrowser,
      filePid,
      "Check that http tab not in file content process after cross origin load."
    );
    ok(
      E10SUtils.isWebRemoteType(httpBrowser),
      "Check that tab now has web remote type."
    );

    // Check that history back now remains in the web content process.
    let httpPid = await ContentTask.spawn(httpBrowser, null, () => {
      return Services.appinfo.processID;
    });
    promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_HTTP
    );
    httpBrowser.goBack();
    await promiseLocation;
    await CheckBrowserInPid(
      httpBrowser,
      httpPid,
      "Check that http tab still in web content process after process switch and history back."
    );

    // Check that history back to file:// URI switches to file content process.
    promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      uriString
    );
    httpBrowser.goBack();
    await promiseLocation;
    await CheckBrowserNotInPid(
      httpBrowser,
      httpPid,
      "Check that history back to file:// URI switches to file content process."
    );
    is(
      httpBrowser.remoteType,
      E10SUtils.FILE_REMOTE_TYPE,
      "Check that tab now has file remote type."
    );
  });
});
