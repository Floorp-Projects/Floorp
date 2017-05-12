/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_FILE = "dummy_page.html";
const TEST_HTTP = "http://example.org/";

// Test for bug 1343184.
add_task(async function() {
  // Open file:// page.
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_FILE);
  const uriString = Services.io.newFileURI(dir).spec;
  await BrowserTestUtils.withNewTab(uriString, async function(fileBrowser) {
    // Set prefs to ensure file content process and to allow linked web content
    // in file URI process.
    await SpecialPowers.pushPrefEnv(
      {set: [["browser.tabs.remote.separateFileUriProcess", true],
             ["browser.tabs.remote.allowLinkedWebInFileUriProcess", true]]});

    // Open new http tab from JavaScript in file:// page.
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, TEST_HTTP);
    await ContentTask.spawn(fileBrowser, TEST_HTTP, uri => {
      content.open(uri, "_blank");
    });
    let httpTab = await promiseTabOpened;
    registerCleanupFunction(async function() {
      await BrowserTestUtils.removeTab(httpTab);
    });

    // Ensure that http browser is running in the same process as the file:// one.
    let filePid = await ContentTask.spawn(fileBrowser, null, () => {
      return Services.appinfo.processID;
    });
    await ContentTask.spawn(httpTab.linkedBrowser, filePid, (expectedPid) => {
      is(Services.appinfo.processID, expectedPid,
         "Check that new http page opened from file loaded in file content process.");
    });
  });
});
