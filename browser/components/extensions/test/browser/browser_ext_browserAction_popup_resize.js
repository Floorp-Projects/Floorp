/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(new URL("head_browserAction.js", gTestPath).href,
                                    this);

add_task(async function testSetup() {
  Services.prefs.setBoolPref("toolkit.cosmeticAnimations.enabled", false);
});

add_task(async function testBrowserActionPopupResize() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },

    files: {
      "popup.html": '<!DOCTYPE html><html><head><meta charset="utf-8"></head></html>',
    },
  });

  await extension.startup();

  let browser = await openBrowserActionPanel(extension, undefined, true);

  async function checkSize(expected) {
    let dims = await promiseContentDimensions(browser);

    Assert.lessOrEqual(Math.abs(dims.window.innerHeight - expected), 1,
                       `Panel window should be ${expected}px tall (was ${dims.window.innerHeight})`);
    is(dims.body.clientHeight, dims.body.scrollHeight,
       "Panel body should be tall enough to fit its contents");

    // Tolerate if it is 1px too wide, as that may happen with the current resizing method.
    Assert.lessOrEqual(Math.abs(dims.window.innerWidth - expected), 1,
                       `Panel window should be ${expected}px wide`);
    is(dims.body.clientWidth, dims.body.scrollWidth,
       "Panel body should be wide enough to fit its contents");
  }

  function setSize(size) {
    content.document.body.style.height = `${size}px`;
    content.document.body.style.width = `${size}px`;
  }

  let sizes = [
    200,
    400,
    300,
  ];

  for (let size of sizes) {
    await alterContent(browser, setSize, size);
    await checkSize(size);
  }

  let popup = getBrowserActionPopup(extension);
  await closeBrowserAction(extension);
  is(popup.state, "closed", "browserAction popup has been closed");

  await extension.unload();
});


add_task(async function testBrowserActionMenuResizeStandards() {
  await testPopupSize(true);
});

add_task(async function testBrowserActionMenuResizeQuirks() {
  await testPopupSize(false);
});

add_task(async function testTeardown() {
  Services.prefs.clearUserPref("toolkit.cosmeticAnimations.enabled");
});
