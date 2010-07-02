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

// This test makes sure that the geolocation prompt does not show a remember
// control inside the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const testPageURL = "http://mochi.test:8888/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_geoprompt_page.html";
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let notification = PopupNotifications.getNotification("geolocation");
    ok(notification, "Notification should exist");
    ok(notification.secondaryActions.length > 1, "Secondary actions should exist (always/never remember)");
    notification.remove();

    gBrowser.removeCurrentTab();

    // enter the private browsing mode
    pb.privateBrowsingEnabled = true;

    gBrowser.selectedTab = gBrowser.addTab();
    gBrowser.selectedBrowser.addEventListener("load", function () {
      gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

      // Make sure the notification is correctly displayed without a remember control
      let notification = PopupNotifications.getNotification("geolocation");
      ok(notification, "Notification should exist");
      is(notification.secondaryActions.length, 0, "Secondary actions shouldn't exist (always/never remember)");
      notification.remove();

      gBrowser.removeCurrentTab();

      // cleanup
      pb.privateBrowsingEnabled = false;
      finish();
    }, true);
    content.location = testPageURL;
  }, true);
  content.location = testPageURL;
}
