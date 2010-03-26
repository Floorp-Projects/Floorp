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

// This test makes sure that about:privatebrowsing does not appear zoomed in
// if there is already a zoom site pref for about:blank (bug 487656).

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  waitForExplicitFinish();

  let tabBlank = gBrowser.selectedTab;
  gBrowser.removeAllTabsBut(tabBlank);

  let blankBrowser = gBrowser.getBrowserForTab(tabBlank);
  blankBrowser.addEventListener("load", function() {
    blankBrowser.removeEventListener("load", arguments.callee, true);

    // change the zoom on the blank page
    FullZoom.enlarge();
    isnot(ZoomManager.zoom, 1, "Zoom level for about:blank should be changed");

    // enter private browsing mode
    pb.privateBrowsingEnabled = true;
    let tabAboutPB = gBrowser.selectedTab;
    let browserAboutPB = gBrowser.getBrowserForTab(tabAboutPB);
    browserAboutPB.addEventListener("load", function() {
      browserAboutPB.removeEventListener("load", arguments.callee, true);
      setTimeout(function() {
        // make sure the zoom level is set to 1
        is(ZoomManager.zoom, 1, "Zoom level for about:privatebrowsing should be reset");
        finishTest();
      }, 0);
    }, true);
  }, true);
  blankBrowser.loadURI("about:blank");
}

function finishTest() {
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  // leave private browsing mode
  pb.privateBrowsingEnabled = false;
  let tabBlank = gBrowser.selectedTab;
  let blankBrowser = gBrowser.getBrowserForTab(tabBlank);
  blankBrowser.addEventListener("load", function() {
    blankBrowser.removeEventListener("load", arguments.callee, true);

    executeSoon(function() {
      // cleanup
      FullZoom.reset();
      finish();
    });
  }, true);
}
