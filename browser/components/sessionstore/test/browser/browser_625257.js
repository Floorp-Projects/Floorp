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

// This tests that a tab which is closed while loading is not lost.
// Specifically, that session store does not rely on an invalid cache when
// constructing data for a tab which is loading.

let ss = Cc["@mozilla.org/browser/sessionstore;1"].
         getService(Ci.nsISessionStore);

// The newly created tab which we load a URL into and try closing/undoing.
let tab;

// |state| tracks the progress of the test in 4 parts:
// -1: Initial value.
//  0: Tab has been created is loading URI_TO_LOAD.
//  1: browser.currentURI has changed and tab is scheduled to be removed.
//  2: undoCloseTab() has been called, tab should fully load.
let state = -1;
const URI_TO_LOAD = "about:home";

function test() {
  waitForExplicitFinish();

  // We'll be waiting for session stores, so we speed up their rate to ensure
  // we don't timeout.
  Services.prefs.setIntPref("browser.sessionstore.interval", 2000);

  gBrowser.addTabsProgressListener(tabsListener);

  waitForSaveState(test_bug625257_1);
  tab = gBrowser.addTab();

  tab.linkedBrowser.addEventListener("load", onLoad, true);

  gBrowser.tabContainer.addEventListener("TabClose", onTabClose, true);
}

// First, the newly created blank tab should trigger a save state.
function test_bug625257_1() {
  is(gBrowser.tabs[1], tab, "newly created tab should exist by now");
  ok(tab.linkedBrowser.__SS_data, "newly created tab should be in save state");

  tab.linkedBrowser.loadURI(URI_TO_LOAD);
  state = 0;
}

let tabsListener = {
  onLocationChange: function onLocationChange(aBrowser) {
    gBrowser.removeTabsProgressListener(tabsListener);
    is(state, 0, "should be the first listener event");
    state++;

    // Since we are running in the context of tabs listeners, we do not
    // want to disrupt other tabs listeners.
    executeSoon(function() {
      tab.linkedBrowser.removeEventListener("load", onLoad, true);
      gBrowser.removeTab(tab);
    });
  }
};

function onTabClose(aEvent) {
  let uri = aEvent.target.location;

  is(state, 1, "should only remove tab at this point");
  state++;
  gBrowser.tabContainer.removeEventListener("TabClose", onTabClose, true);

  executeSoon(function() {
    tab = ss.undoCloseTab(window, 0);
    tab.linkedBrowser.addEventListener("load", onLoad, true);
  });
}

function onLoad(aEvent) {
  let uri = aEvent.target.location;

  if (state == 2) {
    is(uri, URI_TO_LOAD, "should load page from undoCloseTab");
    done();
  }
  else {
    isnot(uri, URI_TO_LOAD, "should not fully load page");
  }
}

function done() {
  tab.linkedBrowser.removeEventListener("load", onLoad, true);
  gBrowser.removeTab(gBrowser.tabs[1]);
  Services.prefs.clearUserPref("browser.sessionstore.interval");

  executeSoon(finish);
}
