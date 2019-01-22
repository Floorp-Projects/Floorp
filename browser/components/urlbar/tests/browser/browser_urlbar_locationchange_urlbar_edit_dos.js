"use strict";

const TEST_PATH = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content", "http://www.example.com");
const TEST_URL = `${TEST_PATH}file_urlbar_edit_dos.html`;

async function checkURLBarValueStays(browser) {
  gURLBar.select();
  EventUtils.sendString("a");
  is(gURLBar.value, "a", "URL bar value should match after sending a key");
  await new Promise(resolve => {
    let listener = {
      onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
        ok(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT,
           "Should only get a same document location change");
        gBrowser.selectedBrowser.removeProgressListener(filter);
        filter = null;
        resolve();
      },
    };
    let filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_ALL);
    gBrowser.selectedBrowser.addProgressListener(filter);
  });
  is(gURLBar.value, "a", "URL bar should not have been changed by location changes.");
}

add_task(async function() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_URL,
  }, async function(browser) {
    await ContentTask.spawn(browser, "", function() {
      content.wrappedJSObject.dos_hash();
    });
    await checkURLBarValueStays(browser);
    await ContentTask.spawn(browser, "", function() {
      content.clearTimeout(content.wrappedJSObject.dos_timeout);
      content.wrappedJSObject.dos_pushState();
    });
    await checkURLBarValueStays(browser);
  });
});
