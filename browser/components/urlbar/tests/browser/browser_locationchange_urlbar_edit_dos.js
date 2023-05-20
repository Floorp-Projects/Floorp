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
        // Wait an extra tick before resolving.  We want to make sure that other
        // web progress listeners queued after this one are called before we
        // continue the test, in case the remainder of the test depends on those
        // listeners.  That should happen anyway since promises are resolved on
        // the next tick, but do this to be a little safer.  In particular we
        // want to avoid having the test pass when it should fail.
        executeSoon(resolve);
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

add_task(async function () {
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
    async function (browser) {
      let promise1 = checkURLBarValueStays(browser);
      SpecialPowers.spawn(browser, [""], function () {
        content.wrappedJSObject.dos_hash();
      });
      await promise1;
      let promise2 = checkURLBarValueStays(browser);
      SpecialPowers.spawn(browser, [""], function () {
        content.wrappedJSObject.dos_pushState();
      });
      await promise2;
    }
  );
});
