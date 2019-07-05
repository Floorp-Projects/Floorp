/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `${TEST_BASE_URL}file_urlbar_edit_dos.html`;

async function checkURLBarValueStays(browser) {
  gURLBar.select();
  EventUtils.sendString("a");
  is(gURLBar.value, "a", "URL bar value should match after sending a key");
  await new Promise(resolve => {
    let listener = {
      onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
        ok(
          aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT,
          "Should only get a same document location change"
        );
        gBrowser.selectedBrowser.removeProgressListener(filter);
        filter = null;
        resolve();
      },
    };
    let filter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_ALL);
    gBrowser.selectedBrowser.addProgressListener(filter);
  });
  is(
    gURLBar.value,
    "a",
    "URL bar should not have been changed by location changes."
  );
}

add_task(async function() {
  // Disable autofill so that when checkURLBarValueStays types "a", it's not
  // autofilled to addons.mozilla.org (or anything else).
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL,
    },
    async function(browser) {
      await ContentTask.spawn(browser, "", function() {
        content.wrappedJSObject.dos_hash();
      });
      await checkURLBarValueStays(browser);
      await ContentTask.spawn(browser, "", function() {
        content.clearTimeout(content.wrappedJSObject.dos_timeout);
        content.wrappedJSObject.dos_pushState();
      });
      await checkURLBarValueStays(browser);
    }
  );
});
