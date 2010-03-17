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
 * The Original Code is Firefox Browser Test Code.
 *
 * The Initial Developer of the Original Code is
 * SHIMODA Hiroshi <piro@p.club.ne.jp>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * "TabClose" event is possibly used for closing related tabs of the current.
 * "removeTab" method should work correctly even if the number of tabs are
 * changed while "TabClose" event.
 */

var count = 0;
const URIS = ["about:config",
              "about:plugins",
              "about:buildconfig",
              "data:text/html,<title>OK</title>"];

function test() {
  waitForExplicitFinish();
  URIS.forEach(addTab);
}

function addTab(aURI, aIndex) {
  var tab = gBrowser.addTab(aURI);
  if (aIndex == 0)
    gBrowser.removeTab(gBrowser.mTabs[0]);

  tab.linkedBrowser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);
    if (++count == URIS.length)
      executeSoon(doTabsTest);
  }, true);
}

function doTabsTest() {
  is(gBrowser.mTabs.length, URIS.length, "Correctly opened all expected tabs");

  // sample of "close related tabs" feature
  gBrowser.tabContainer.addEventListener("TabClose", function (event) {
    event.currentTarget.removeEventListener("TabClose", arguments.callee, true);
    var closedTab = event.originalTarget;
    var scheme = closedTab.linkedBrowser.currentURI.scheme;
    Array.slice(gBrowser.mTabs).forEach(function (aTab) {
      if (aTab != closedTab && aTab.linkedBrowser.currentURI.scheme == scheme)
        gBrowser.removeTab(aTab);
    });
  }, true);

  gBrowser.removeTab(gBrowser.mTabs[0]);
  is(gBrowser.mTabs.length, 1, "Related tabs are not closed unexpectedly");

  gBrowser.addTab("about:blank");
  gBrowser.removeTab(gBrowser.mTabs[0]);
  finish();
}
