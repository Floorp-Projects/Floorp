/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the nsISecureBrowserUI implementation doesn't send extraneous OnSecurityChange events
// when it receives OnLocationChange events with the LOCATION_CHANGE_SAME_DOCUMENT flag set.

add_task(async function () {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let onLocationChangeCount = 0;
    let onSecurityChangeCount = 0;
    let progressListener = {
      onStateChange() {},
      onLocationChange() {
        onLocationChangeCount++;
      },
      onSecurityChange() {
        onSecurityChangeCount++;
      },
      onProgressChange() {},
      onStatusChange() {},

      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };
    browser.addProgressListener(progressListener, Ci.nsIWebProgress.NOTIFY_ALL);

    let uri =
      getRootDirectory(gTestPath).replace(
        "chrome://mochitests/content",
        "https://example.com"
      ) + "dummy_page.html";
    BrowserTestUtils.startLoadingURIString(browser, uri);
    await BrowserTestUtils.browserLoaded(browser, false, uri);
    is(onLocationChangeCount, 1, "should have 1 onLocationChange event");
    is(onSecurityChangeCount, 1, "should have 1 onSecurityChange event");
    await SpecialPowers.spawn(browser, [], async () => {
      content.history.pushState({}, "", "https://example.com");
    });
    is(onLocationChangeCount, 2, "should have 2 onLocationChange events");
    is(
      onSecurityChangeCount,
      1,
      "should still have only 1 onSecurityChange event"
    );
  });
});
