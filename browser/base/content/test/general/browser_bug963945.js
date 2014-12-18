/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test ensures the about:addons tab is only
 * opened one time when in private browsing.
 */

function test() {
  waitForExplicitFinish();
	
  var win = OpenBrowserWindow({private: true});

  whenDelayedStartupFinished(win, function() {
    win.gBrowser.loadURI("about:addons");

    waitForFocus(function() {
      EventUtils.synthesizeKey("a", { ctrlKey: true, shiftKey: true }, win);

      is(win.gBrowser.tabs.length, 1, "about:addons tab was re-focused.");
      is(win.gBrowser.currentURI.spec, "about:addons", "Addons tab was opened.");

      win.close();
      finish();    
    });
  });
}


