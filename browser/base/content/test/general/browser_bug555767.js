    /* This Source Code Form is subject to the terms of the Mozilla Public
     * License, v. 2.0. If a copy of the MPL was not distributed with this
     * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    add_task(async function() {
      let testURL = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
      let tabSelected = false;

      // Open the base tab
      let baseTab = BrowserTestUtils.addTab(gBrowser, testURL);

      // Wait for the tab to be fully loaded so matching happens correctly
      await promiseTabLoaded(baseTab);
      if (baseTab.linkedBrowser.currentURI.spec == "about:blank")
        return;
      baseTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

      let testTab = BrowserTestUtils.addTab(gBrowser);

      // Select the testTab
      gBrowser.selectedTab = testTab;

      // Set the urlbar to include the moz-action
      gURLBar.value = "moz-action:switchtab," + JSON.stringify({url: testURL});
      // Focus the urlbar so we can press enter
      gURLBar.focus();

      // Functions for TabClose and TabSelect
      function onTabClose(aEvent) {
        gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);
        // Make sure we get the TabClose event for testTab
        is(aEvent.originalTarget, testTab, "Got the TabClose event for the right tab");
        // Confirm that we did select the tab
        ok(tabSelected, "Confirming that the tab was selected");
        gBrowser.removeTab(baseTab);
        finish();
      }
      function onTabSelect(aEvent) {
        gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect);
        // Make sure we got the TabSelect event for baseTab
        is(aEvent.originalTarget, baseTab, "Got the TabSelect event for the right tab");
        // Confirm that the selected tab is in fact base tab
        is(gBrowser.selectedTab, baseTab, "We've switched to the correct tab");
        tabSelected = true;
      }

      // Add the TabClose, TabSelect event listeners before we press enter
      gBrowser.tabContainer.addEventListener("TabClose", onTabClose);
      gBrowser.tabContainer.addEventListener("TabSelect", onTabSelect);

      // Press enter!
      EventUtils.synthesizeKey("VK_RETURN", {});
    });

