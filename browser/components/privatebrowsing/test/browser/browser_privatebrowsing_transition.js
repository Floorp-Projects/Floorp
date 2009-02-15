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
 *   Nochum Sossonko <nsossonko@hotmail.com> (Original Author)
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
 
// Tests the order of events and notifications when entering/exiting private
// browsing mode. This ensures that all private data is removed on exit, e.g.
// a cookie set in on unload handler, see bug 476463.
let cookieManager = Cc["@mozilla.org/cookiemanager;1"].
                    getService(Ci.nsICookieManager2);
let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);
let _obs = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
let observerNotified = 0,  firstUnloadFired = 0, secondUnloadFired = 0;

let pbObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "private-browsing") {
      switch(aData) {
        case "enter":
          observerNotified++;
          is(observerNotified, 1, "This should be the first notification");
          is(firstUnloadFired, 1, "The first unload event should have been processed by now");
          break;
        case "exit":
          _obs.removeObserver(this, "private-browsing");
          observerNotified++;
          is(observerNotified, 2, "This should be the second notification");
          is(secondUnloadFired, 1, "The second unload event should have been processed by now");
          break;
      }
    }
  }
}
function test() {
  waitForExplicitFinish();
  _obs.addObserver(pbObserver, "private-browsing", false);
  is(gBrowser.tabContainer.childNodes.length, 1, "There should only be one tab");
  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  testTab.linkedBrowser.addEventListener("unload", (function() {
    testTab.linkedBrowser.removeEventListener("unload", arguments.callee, true);
    firstUnloadFired++;
    is(observerNotified, 0, "The notification shouldn't have been sent yet");
  }), true);

  pb.privateBrowsingEnabled = true;
  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  testTab.linkedBrowser.addEventListener("unload", (function() {
    testTab.linkedBrowser.removeEventListener("unload", arguments.callee, true);
    secondUnloadFired++;
    is(observerNotified, 1, "The notification shouldn't have been sent yet");
    cookieManager.add("example.com", "test/", "PB", "1", false, false, false, 1000000000000);
  }), true);

  pb.privateBrowsingEnabled = false;
  gBrowser.tabContainer.lastChild.linkedBrowser.addEventListener("unload", (function() {
    gBrowser.tabContainer.lastChild.linkedBrowser.removeEventListener("unload", arguments.callee, true);
    let count = cookieManager.countCookiesFromHost("example.com");
    is(count, 0, "There shouldn't be any cookies once pb mode has exited");
    cookieManager.QueryInterface(Ci.nsICookieManager);
    cookieManager.remove("example.com", "PB", "test/", false);
  }), true);
  gBrowser.removeCurrentTab();
  finish();
}
