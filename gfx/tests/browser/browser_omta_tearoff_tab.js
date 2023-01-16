/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var testPage =
  "data:text/html,<head><style>body{animation: fadein 1s infinite;} @keyframes fadein{from{opacity: 0;}}</style><body>Text";

add_task(async function test() {
  let tab = BrowserTestUtils.addTab(gBrowser, testPage, {
    skipAnimation: true,
  });
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.switchTab(gBrowser, tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const anim = content.document.getAnimations()[0];
    await anim.ready;
    ok(SpecialPowers.wrap(anim).isRunningOnCompositor);
  });

  let promiseWin = BrowserTestUtils.waitForNewWindow();
  let newWin = gBrowser.replaceTabWithWindow(tab);
  await promiseWin;
  Assert.ok(
    ChromeUtils.vsyncEnabled(),
    "vsync should be enabled as we have a tab with an animation"
  );

  newWin.close();
  await TestUtils.waitForCondition(
    () => !ChromeUtils.vsyncEnabled(),
    "wait for vsync to be disabled"
  );
  Assert.ok(
    !ChromeUtils.vsyncEnabled(),
    "vsync should be disabled after closing window that contained an animated tab"
  );
});
