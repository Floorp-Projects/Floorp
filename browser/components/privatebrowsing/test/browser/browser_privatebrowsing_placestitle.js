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
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
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

// This test makes sure that the title of existing history entries does not
// change inside the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let cm = Cc["@mozilla.org/cookiemanager;1"].
           getService(Ci.nsICookieManager);
  waitForExplicitFinish();

  const TEST_URL = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/title.sjs";

  function waitForCleanup(aCallback) {
    // delete all cookies
    cm.removeAll();
    // delete all history items
    waitForClearHistory(aCallback);
  }

  let observer = {
    pass: 1,
    onTitleChanged: function(aURI, aPageTitle) {
      if (aURI.spec != TEST_URL)
        return;
      switch (this.pass++) {
      case 1: // the first time that the page is loaded
        is(aPageTitle, "No Cookie", "The page should be loaded without any cookie for the first time");
        gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
        break;
      case 2: // the second time that the page is loaded
        is(aPageTitle, "Cookie", "The page should be loaded with a cookie for the second time");
        waitForCleanup(function () {
          gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
        });
        break;
      case 3: // before entering the private browsing mode
        is(aPageTitle, "No Cookie", "The page should be loaded without any cookie again");
        // enter private browsing mode
        pb.privateBrowsingEnabled = true;
        gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
        executeSoon(function() {
          PlacesUtils.history.removeObserver(observer);
          pb.privateBrowsingEnabled = false;
          while (gBrowser.browsers.length > 1) {
            gBrowser.removeCurrentTab();
          }
          waitForCleanup(finish);
        });
        break;
      default:
        ok(false, "Unexpected pass: " + (this.pass - 1));
      }
    },

    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onVisit: function () {},
    onBeforeDeleteURI: function () {},
    onDeleteURI: function () {},
    onClearHistory: function () {},
    onPageChanged: function () {},
    onDeleteVisits: function() {},

    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };
  PlacesUtils.history.addObserver(observer, false);

  waitForCleanup(function () {
    gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
  });
}
