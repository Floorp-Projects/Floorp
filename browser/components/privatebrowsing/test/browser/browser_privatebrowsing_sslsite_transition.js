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

// This test makes sure that SSL sites load correctly after leaving the
// Private Browsing mode (bug 463256 and bug 496335).

function test() {
  // initialization
  let prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const kTestURL = "https://example.com/";

  // load an SSL site in the first tab and wait for it to finish loading
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    pb.privateBrowsingEnabled = true;
    pb.privateBrowsingEnabled = false;

    // Note: if the page fails to load, the test will time out
    browser.addEventListener("load", function() {
      browser.removeEventListener("load", arguments.callee, true);

      is(browser.contentWindow.location, kTestURL,
        "The original SSL page should be loaded at this stage");

      gBrowser.removeTab(tab);
      prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
      finish();
    }, true);

    executeSoon(function(){
      browser.contentWindow.location = kTestURL;
    });
  }, true);
  browser.contentWindow.location = kTestURL;

  waitForExplicitFinish();
}
