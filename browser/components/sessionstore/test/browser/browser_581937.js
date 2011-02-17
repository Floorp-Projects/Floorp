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
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Mehdi Mulani <mmmulani@uwaterloo.ca>
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
 * ***** END LICENSE BLOCK *****/

 // Tests that an about:blank tab with no history will not be saved into
 // session store and thus, it will not show up in Recently Closed Tabs.

 let ss = Cc["@mozilla.org/browser/sessionstore;1"].
          getService(Ci.nsISessionStore);

let tab;
function test() {
  waitForExplicitFinish();

  gPrefService.setIntPref("browser.sessionstore.max_tabs_undo", 0);
  gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");

  is(ss.getClosedTabCount(window), 0, "should be no closed tabs");

  gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, true);

  tab = gBrowser.addTab();
}

function onTabOpen(aEvent) {
  gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen, true);

  // Let other listeners react to the TabOpen event before removing the tab.
  executeSoon(function() {
    is(gBrowser.browsers[1].currentURI.spec, "about:blank",
       "we will be removing an about:blank tab");

    gBrowser.removeTab(tab);

    is(ss.getClosedTabCount(window), 0, "should still be no closed tabs");

    executeSoon(finish);
  });
}
