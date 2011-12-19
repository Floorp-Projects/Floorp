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

// The newly created tab which we load a URL into and try closing/undoing.
let tab;

// This test steps through the following parts:
//  1. Tab has been created is loading URI_TO_LOAD.
//  2. Before URI_TO_LOAD finishes loading, browser.currentURI has changed and
//     tab is scheduled to be removed.
//  3. After the tab has been closed, undoCloseTab() has been called and the tab
//     should fully load.
const URI_TO_LOAD = "about:home";

function test() {
  waitForExplicitFinish();

  gBrowser.addTabsProgressListener(tabsListener);

  tab = gBrowser.addTab();

  tab.linkedBrowser.addEventListener("load", firstOnLoad, true);

  gBrowser.tabContainer.addEventListener("TabClose", onTabClose, true);
}

function firstOnLoad(aEvent) {
  tab.linkedBrowser.removeEventListener("load", firstOnLoad, true);

  let uri = aEvent.target.location;
  is(uri, "about:blank", "first load should be for about:blank");

  // Trigger a save state.
  ss.getBrowserState();

  is(gBrowser.tabs[1], tab, "newly created tab should exist by now");
  ok(tab.linkedBrowser.__SS_data, "newly created tab should be in save state");

  tab.linkedBrowser.loadURI(URI_TO_LOAD);
}

let tabsListener = {
  onLocationChange: function onLocationChange(aBrowser) {
    gBrowser.removeTabsProgressListener(tabsListener);

    is(aBrowser.currentURI.spec, URI_TO_LOAD,
       "should occur after about:blank load and be loading next page");

    // Since we are running in the context of tabs listeners, we do not
    // want to disrupt other tabs listeners.
    executeSoon(function() {
      gBrowser.removeTab(tab);
    });
  }
};

function onTabClose(aEvent) {
  gBrowser.tabContainer.removeEventListener("TabClose", onTabClose, true);

  is(tab.linkedBrowser.currentURI.spec, URI_TO_LOAD,
     "should only remove when loading page");

  executeSoon(function() {
    tab = ss.undoCloseTab(window, 0);
    tab.linkedBrowser.addEventListener("load", secondOnLoad, true);
  });
}

function secondOnLoad(aEvent) {
  let uri = aEvent.target.location;
  is(uri, URI_TO_LOAD, "should load page from undoCloseTab");
  done();
}

function done() {
  tab.linkedBrowser.removeEventListener("load", secondOnLoad, true);
  gBrowser.removeTab(tab);

  executeSoon(finish);
}
