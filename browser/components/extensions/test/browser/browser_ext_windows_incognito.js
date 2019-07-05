/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_window_incognito() {
  SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  const url =
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/file_iframe_document.html";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://mochi.test/"],
    },
    background() {
      browser.test.onMessage.addListener(async pbw => {
        await browser.test.assertRejects(
          browser.windows.get(pbw.windowId),
          /Invalid window ID/,
          "should not be able to get incognito window"
        );
        await browser.test.assertRejects(
          browser.windows.remove(pbw.windowId),
          /Invalid window ID/,
          "should not be able to remove incognito window"
        );
        await browser.test.assertRejects(
          browser.windows.getCurrent(),
          /Invalid window/,
          "should not be able to get incognito top window"
        );
        await browser.test.assertRejects(
          browser.windows.getLastFocused(),
          /Invalid window/,
          "should not be able to get incognito focused window"
        );
        await browser.test.assertRejects(
          browser.windows.create({ incognito: true }),
          /Extension does not have permission for incognito mode/,
          "should not be able to create incognito window"
        );
        await browser.test.assertRejects(
          browser.windows.update(pbw.windowId, { focused: true }),
          /Invalid window ID/,
          "should not be able to update incognito window"
        );

        let windows = await browser.windows.getAll();
        browser.test.assertEq(
          1,
          windows.length,
          "unable to get incognito window"
        );

        browser.test.notifyPass("pass");
      });
    },
  });

  let winData = await getIncognitoWindow(url);

  await extension.startup();
  extension.sendMessage(winData.details);
  await extension.awaitFinish("pass");
  await BrowserTestUtils.closeWindow(winData.win);
  await extension.unload();
});
