/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(new URL("head_browserAction.js", gTestPath).href,
                                    this);

add_task(async function testSetup() {
  Services.prefs.setBoolPref("toolkit.cosmeticAnimations.enabled", false);
});

// Test that we still make reasonable maximum size calculations when the window
// is close enough to the bottom of the screen that the menu panel opens above,
// rather than below, its button.
add_task(async function testBrowserActionMenuResizeBottomArrow() {
  const WIDTH = 800;
  const HEIGHT = 80;

  let left = screen.availLeft + screen.availWidth - WIDTH;
  let top = screen.availTop + screen.availHeight - HEIGHT;

  let win = await BrowserTestUtils.openNewBrowserWindow();

  win.resizeTo(WIDTH, HEIGHT);

  // Sometimes we run into problems on Linux with resizing being asynchronous
  // and window managers not allowing us to move the window so that any part of
  // it is off-screen, so we need to try more than once.
  for (let i = 0; i < 20; i++) {
    win.moveTo(left, top);

    if (win.screenX == left && win.screenY == top) {
      break;
    }

    await delay(100);
  }

  await SimpleTest.promiseFocus(win);

  await testPopupSize(true, win, "bottom");

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testTeardown() {
  Services.prefs.clearUserPref("toolkit.cosmeticAnimations.enabled");
});
