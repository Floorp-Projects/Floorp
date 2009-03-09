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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

// This test makes sure that private browsing turns off doesn't cause zoom
// settings to be reset on tab switch (bug 464962)

function test() {
  // initialization
  let prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  let tabAbout = gBrowser.addTab();
  gBrowser.selectedTab = tabAbout;

  waitForExplicitFinish();

  let aboutBrowser = gBrowser.getBrowserForTab(tabAbout);
  aboutBrowser.addEventListener("load", function () {
    aboutBrowser.removeEventListener("load", arguments.callee, true);
    let tabRobots = gBrowser.addTab();
    gBrowser.selectedTab = tabRobots;

    let robotsBrowser = gBrowser.getBrowserForTab(tabRobots);
    robotsBrowser.addEventListener("load", function () {
      robotsBrowser.removeEventListener("load", arguments.callee, true);
      let robotsZoom = ZoomManager.zoom;

      // change the zoom on the robots page
      FullZoom.enlarge();
      // make sure the zoom level has been changed
      isnot(ZoomManager.zoom, robotsZoom, "Zoom level can be changed");
      robotsZoom = ZoomManager.zoom;

      // switch to about: tab
      gBrowser.selectedTab = tabAbout;

      // switch back to robots tab
      gBrowser.selectedTab = tabRobots;

      // make sure the zoom level has not changed
      is(ZoomManager.zoom, robotsZoom,
        "Entering private browsing should not reset the zoom on a tab");

      // leave private browsing mode
      pb.privateBrowsingEnabled = false;

      // cleanup
      prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
      FullZoom.reset();
      gBrowser.removeTab(tabRobots);
      gBrowser.removeTab(tabAbout);
      finish();
    }, true);
    robotsBrowser.contentWindow.location = "about:robots";
  }, true);
  aboutBrowser.contentWindow.location = "about:";
}
