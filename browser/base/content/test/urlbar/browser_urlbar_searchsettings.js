/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function*() {
  let button = document.getElementById("urlbar-search-settings");
  if (!button) {
    ok("Skipping test");
    return;
  }

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* () {
    let popupopened = BrowserTestUtils.waitForEvent(gURLBar.popup, "popupshown");

    gURLBar.focus();
    EventUtils.synthesizeKey("a", {});
    yield popupopened;

    // Since the current tab is blank the preferences pane will load there
    let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    let popupclosed = BrowserTestUtils.waitForEvent(gURLBar.popup, "popuphidden");
    EventUtils.synthesizeMouseAtCenter(button, {});
    yield loaded;
    yield popupclosed;

    is(gBrowser.selectedBrowser.currentURI.spec, "about:preferences#search",
       "Should have loaded the right page");
  });
});
