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
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Nochum Sossonko.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nochum Sossonko <highmind63@gmail.com> (Original Author)
 *   Ehsan Akhgari <ehsan@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// This test makes sure that cancelling the unloading of a page with a beforeunload
// handler prevents the private browsing mode transition.

function test() {
  const TEST_PAGE_1 = "data:text/html,<body%20onbeforeunload='return%20false;'>first</body>";
  const TEST_PAGE_2 = "data:text/html,<body%20onbeforeunload='return%20false;'>second</body>";
  let pb = Cc["@mozilla.org/privatebrowsing;1"]
             .getService(Ci.nsIPrivateBrowsingService);

  let rejectDialog = 0;
  let acceptDialog = 0;
  let confirmCalls = 0;
  function promptObserver(aSubject, aTopic, aData) {
    let dialogWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
    confirmCalls++;
    if (acceptDialog-- > 0)
      dialogWin.document.documentElement.getButton("accept").click();
    else if (rejectDialog-- > 0)
      dialogWin.document.documentElement.getButton("cancel").click();
  }

  Services.obs.addObserver(promptObserver, "common-dialog-loaded", false);

  waitForExplicitFinish();
  let browser1 = gBrowser.getBrowserForTab(gBrowser.addTab());
  browser1.addEventListener("load", function() {
    browser1.removeEventListener("load", arguments.callee, true);

    let browser2 = gBrowser.getBrowserForTab(gBrowser.addTab());
    browser2.addEventListener("load", function() {
      browser2.removeEventListener("load", arguments.callee, true);

      rejectDialog = 1;
      pb.privateBrowsingEnabled = true;

      ok(!pb.privateBrowsingEnabled, "Private browsing mode should not have been activated");
      is(confirmCalls, 1, "Only one confirm box should be shown");
      is(gBrowser.tabs.length, 3,
         "No tabs should be closed because private browsing mode transition was canceled");
      is(gBrowser.getBrowserForTab(gBrowser.tabContainer.firstChild).currentURI.spec, "about:blank",
         "The first tab should be a blank tab");
      is(gBrowser.getBrowserForTab(gBrowser.tabContainer.firstChild.nextSibling).currentURI.spec, TEST_PAGE_1,
         "The middle tab should be the same one we opened");
      is(gBrowser.getBrowserForTab(gBrowser.tabContainer.lastChild).currentURI.spec, TEST_PAGE_2,
         "The last tab should be the same one we opened");
      is(rejectDialog, 0, "Only one confirm dialog should have been rejected");

      confirmCalls = 0;
      acceptDialog = 2;
      pb.privateBrowsingEnabled = true;

      ok(pb.privateBrowsingEnabled, "Private browsing mode should have been activated");
      is(confirmCalls, 2, "Only two confirm boxes should be shown");
      is(gBrowser.tabs.length, 1,
         "Incorrect number of tabs after transition into private browsing");
      gBrowser.selectedBrowser.addEventListener("load", function() {
        gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

        is(gBrowser.currentURI.spec, "about:privatebrowsing",
           "Incorrect page displayed after private browsing transition");
        is(acceptDialog, 0, "Two confirm dialogs should have been accepted");

        gBrowser.addTab();
        gBrowser.removeTab(gBrowser.selectedTab);
        Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
        pb.privateBrowsingEnabled = false;
        Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
        Services.obs.removeObserver(promptObserver, "common-dialog-loaded", false);
        gBrowser.getBrowserAtIndex(gBrowser.tabContainer.selectedIndex).contentWindow.focus();
        finish();
      }, true);
    }, true);
    browser2.loadURI(TEST_PAGE_2);
  }, true);
  browser1.loadURI(TEST_PAGE_1);
}
