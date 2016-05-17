/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the toolbar button states.
 */

"use strict";

Components.utils.import("resource://gre/modules/Promise.jsm", this);
Services.prefs.setIntPref("loop.gettingStarted.latestFTUVersion", 2);

registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("loop.gettingStarted.latestFTUVersion");
});

add_task(function* test_panelToggle_on_menuitem_click() {
  // Since we _know_ the first click on the button will open the panel, we'll
  // open it using the test utility and check the correct state by clicking the
  // button. This should hide the panel.
  // If we'd open the panel with a simulated click on the button, we won't know
  // for sure when the panel has opened, because the event loop spins a few times
  // in the mean time.
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.panel.state, "open", "Panel should be open");
  // The panel should now be visible. Clicking the menubutton should hide it.
  document.getElementById("menu_openLoop").click();
  Assert.strictEqual(LoopUI.panel.state, "closed", "Panel should be closed");
});

add_task(function* test_private_browsing_window() {
  let win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  let menuItem = win.document.getElementById("menu_openLoop");
  Assert.ok(!menuItem, "Loop menuitem should not be present");

  yield BrowserTestUtils.closeWindow(win);
});
