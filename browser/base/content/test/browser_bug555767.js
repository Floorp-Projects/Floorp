/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let testURL = "http://example.org/browser/browser/base/content/test/dummy_page.html";
  let tabSelected = false;

  // Open the base tab
  let baseTab = gBrowser.addTab(testURL);
  baseTab.linkedBrowser.addEventListener("load", function() {
    // Wait for the tab to be fully loaded so matching happens correctly
    if (baseTab.linkedBrowser.currentURI.spec == "about:blank")
      return;
    baseTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    let testTab = gBrowser.addTab();

    // Select the testTab
    gBrowser.selectedTab = testTab;

    // Ensure that this tab has no history entries
    ok(testTab.linkedBrowser.sessionHistory.count < 2,
       "The test tab has 1 or less history entries");
    // Ensure that this tab is on about:blank
    is(testTab.linkedBrowser.currentURI.spec, "about:blank",
       "The test tab is on about:blank");
    // Ensure that this tab's document has no child nodes
    ok(!testTab.linkedBrowser.contentDocument.body.hasChildNodes(),
       "The test tab has no child nodes");
    ok(!testTab.hasAttribute("busy"),
       "The test tab doesn't have the busy attribute");

    // Set the urlbar to include the moz-action
    gURLBar.value = "moz-action:switchtab," + testURL;
    // Focus the urlbar so we can press enter
    gURLBar.focus();

    // Functions for TabClose and TabSelect
    function onTabClose(aEvent) {
      gBrowser.tabContainer.removeEventListener("TabClose", onTabClose, false);
      // Make sure we get the TabClose event for testTab
      is(aEvent.originalTarget, testTab, "Got the TabClose event for the right tab");
      // Confirm that we did select the tab
      ok(tabSelected, "Confirming that the tab was selected");
      gBrowser.removeTab(baseTab);
      finish();
    }
    function onTabSelect(aEvent) {
      gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect, false);
      // Make sure we got the TabSelect event for baseTab
      is(aEvent.originalTarget, baseTab, "Got the TabSelect event for the right tab");
      // Confirm that the selected tab is in fact base tab
      is(gBrowser.selectedTab, baseTab, "We've switched to the correct tab");
      tabSelected = true;
    }

    // Add the TabClose, TabSelect event listeners before we press enter
    gBrowser.tabContainer.addEventListener("TabClose", onTabClose, false);
    gBrowser.tabContainer.addEventListener("TabSelect", onTabSelect, false);

    // Press enter!
    EventUtils.synthesizeKey("VK_RETURN", {});
  }, true);
}

