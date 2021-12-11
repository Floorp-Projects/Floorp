/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests ensures the urlbar reflects the correct value if a page load is
 * stopped immediately after loading.
 */

"use strict";

const goodURL = "http://mochi.test:8888/";
const badURL = "http://mochi.test:8888/whatever.html";

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, goodURL);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  is(
    gURLBar.value,
    BrowserUIUtils.trimURL(goodURL),
    "location bar reflects loaded page"
  );

  await typeAndSubmitAndStop(badURL);
  is(
    gURLBar.value,
    BrowserUIUtils.trimURL(goodURL),
    "location bar reflects loaded page after stop()"
  );
  gBrowser.removeCurrentTab();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  is(gURLBar.value, "", "location bar is empty");

  await typeAndSubmitAndStop(badURL);
  is(
    gURLBar.value,
    BrowserUIUtils.trimURL(badURL),
    "location bar reflects stopped page in an empty tab"
  );
  gBrowser.removeCurrentTab();
});

async function typeAndSubmitAndStop(url) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: url,
    fireInputEvent: true,
  });

  let docLoadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(
    url,
    gBrowser.selectedBrowser
  );

  // When the load is stopped, tabbrowser calls gURLBar.setURI and then calls
  // onStateChange on its progress listeners.  So to properly wait until the
  // urlbar value has been updated, add our own progress listener here.
  let progressPromise = new Promise(resolve => {
    let listener = {
      onStateChange(browser, webProgress, request, stateFlags, status) {
        if (
          webProgress.isTopLevel &&
          stateFlags & Ci.nsIWebProgressListener.STATE_STOP
        ) {
          gBrowser.removeTabsProgressListener(listener);
          resolve();
        }
      },
    };
    gBrowser.addTabsProgressListener(listener);
  });

  gURLBar.handleCommand();
  await Promise.all([docLoadPromise, progressPromise]);
}
