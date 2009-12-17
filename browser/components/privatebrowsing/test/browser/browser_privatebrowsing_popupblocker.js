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

// This test makes sure that private browsing mode disables the remember option
// for the popup blocker menu.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  let oldPopupPolicy = gPrefService.getBoolPref("dom.disable_open_during_load");
  gPrefService.setBoolPref("dom.disable_open_during_load", true);

  const TEST_URI = "http://localhost:8888/browser/browser/components/privatebrowsing/test/browser/popup.html";

  waitForExplicitFinish();

  function testPopupBlockerMenuItem(expectedDisabled, callback) {
    gBrowser.addEventListener("DOMUpdatePageReport", function() {
      gBrowser.removeEventListener("DOMUpdatePageReport", arguments.callee, false);
      executeSoon(function() {
        let pageReportButton = document.getElementById("page-report-button");
        let notification = gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked");

        ok(!pageReportButton.hidden, "The page report button should not be hidden");
        ok(notification, "The notification box should be displayed");

        function checkMenuItem(callback) {
          document.addEventListener("popupshown", function(event) {
            document.removeEventListener("popupshown", arguments.callee, false);

            if (expectedDisabled)
              is(document.getElementById("blockedPopupAllowSite").getAttribute("disabled"), "true",
                 "The allow popups menu item should be disabled");

            event.originalTarget.hidePopup();
            callback();
          }, false);
        }

        checkMenuItem(function() {
          checkMenuItem(function() {
            gBrowser.removeTab(tab);
            callback();
          });
          notification.querySelector("button").doCommand();
        });
        EventUtils.synthesizeMouse(document.getElementById("page-report-button"), 1, 1, {});
      });
    }, false);

    let tab = gBrowser.addTab(TEST_URI);
    gBrowser.selectedTab = tab;
  }

  testPopupBlockerMenuItem(false, function() {
    pb.privateBrowsingEnabled = true;
    testPopupBlockerMenuItem(true, function() {
      pb.privateBrowsingEnabled = false;
      testPopupBlockerMenuItem(false, function() {
        gPrefService.setBoolPref("dom.disable_open_during_load", oldPopupPolicy);
        finish();
      });
    });
  });
}
