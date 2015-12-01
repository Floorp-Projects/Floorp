/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  // establish initial state
  is(gBrowser.tabs.length, 1, "we start with one tab");
  
  // create a tab
  let tab = gBrowser.loadOneTab("about:blank");
  ok(!tab.hidden, "tab starts out not hidden");
  is(gBrowser.tabs.length, 2, "we now have two tabs");

  // make sure .hidden is read-only
  tab.hidden = true; 
  ok(!tab.hidden, "can't set .hidden directly");

  // hide the tab
  gBrowser.hideTab(tab);
  ok(tab.hidden, "tab is hidden");
  
  // now pin it and make sure it gets unhidden
  gBrowser.pinTab(tab);
  ok(tab.pinned, "tab was pinned");
  ok(!tab.hidden, "tab was unhidden");
  
  // try hiding it now that it's pinned; shouldn't be able to
  gBrowser.hideTab(tab);
  ok(!tab.hidden, "tab did not hide");
    
  // clean up
  gBrowser.removeTab(tab);
  is(gBrowser.tabs.length, 1, "we finish with one tab");

  finish();
}
