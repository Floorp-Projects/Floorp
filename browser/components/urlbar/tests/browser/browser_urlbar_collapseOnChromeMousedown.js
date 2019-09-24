/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MEGABAR_PREF = "browser.urlbar.megabar";

add_task(async function() {
  Services.prefs.setBoolPref(MEGABAR_PREF, true);
  // We need to open a new window for the Megabar to kick in. This can be
  // removed when the Megabar is on by default.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(() => {
    BrowserTestUtils.closeWindow(win);
    Services.prefs.clearUserPref(MEGABAR_PREF);
  });

  win.gURLBar.focus();
  Assert.ok(
    win.gURLBar.hasAttribute("breakout-extend"),
    "The Urlbar should have the breakout-extend attribute."
  );
  EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, {}, win);
  Assert.ok(
    win.gURLBar.hasAttribute("breakout-extend"),
    "The Urlbar should have the breakout-extend attribute."
  );

  // We just want a non-interactive part of the UI.
  let chromeTarget = gURLBar.textbox
    .closest("toolbar")
    .querySelector("toolbarspring");
  EventUtils.synthesizeMouseAtCenter(chromeTarget, {}, win);
  Assert.ok(
    !win.gURLBar.hasAttribute("breakout-extend"),
    "The Urlbar should not have the breakout-extend attribute."
  );
  Assert.ok(win.gURLBar.focused, "The Urlbar should be focused.");

  // Simulating a user switching out of the Firefox window and back in.
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.closeWindow(newWin);
  Assert.ok(
    win.gURLBar.hasAttribute("breakout-extend"),
    "The Urlbar should have the breakout-extend attribute."
  );
});
