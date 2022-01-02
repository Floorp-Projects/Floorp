/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function() {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "a",
      });

      // Since the current tab is blank the preferences pane will load there
      let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      await UrlbarTestUtils.promisePopupClose(window, () => {
        let button = document.getElementById("urlbar-anon-search-settings");
        EventUtils.synthesizeMouseAtCenter(button, {});
      });
      await loaded;

      is(
        gBrowser.selectedBrowser.currentURI.spec,
        "about:preferences#search",
        "Should have loaded the right page"
      );
    }
  );
});
