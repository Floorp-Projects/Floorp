/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is bug 555767 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul O’Shannessy <paul@oshannessy.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

