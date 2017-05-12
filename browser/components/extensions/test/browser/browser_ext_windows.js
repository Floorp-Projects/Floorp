/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testWindowGetAll() {
  let raisedWin = Services.ww.openWindow(
    null, Services.prefs.getCharPref("browser.chromeURL"), "_blank",
    "chrome,dialog=no,all,alwaysRaised", null);

  await TestUtils.topicObserved("browser-delayed-startup-finished",
                                subject => subject == raisedWin);

  let extension = ExtensionTestUtils.loadExtension({
    background: function() {
      browser.windows.getAll((wins) => {
        browser.test.assertEq(wins.length, 2, "Expect two windows");

        browser.test.assertEq(false, wins[0].alwaysOnTop,
                              "Expect first window not to be always on top");
        browser.test.assertEq(true, wins[1].alwaysOnTop,
                              "Expect first window to be always on top");

        browser.test.notifyPass("alwaysOnTop");
      });
    },
  });

  await extension.startup();
  await extension.awaitFinish("alwaysOnTop");
  await extension.unload();

  await BrowserTestUtils.closeWindow(raisedWin);
});

add_task(async function testInvalidWindowId() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.test.assertRejects(
        // Assuming that this windowId does not exist.
        browser.windows.get(123456789),
        /Invalid window/,
        "Should receive invalid window");
      browser.test.notifyPass("windows.get.invalid");
    },
  });

  await extension.startup();
  await extension.awaitFinish("windows.get.invalid");
  await extension.unload();
});
