/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function perma_private_browsing_mode() {
  // make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });

  Assert.equal(
    Services.prefs.getBoolPref("browser.privatebrowsing.autostart"),
    true,
    "Permanent private browsing is enabled"
  );

  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      permissions: ["tabs", "cookies"],
    },
    async background() {
      let win = await browser.windows.create({});
      browser.test.assertTrue(
        win.incognito,
        "New window should be private when perma-PBM is enabled."
      );
      await browser.test.assertRejects(
        browser.tabs.create({
          cookieStoreId: "firefox-container-1",
          windowId: win.id,
        }),
        /Illegal to set non-private cookieStoreId in a private window/,
        "should refuse to open container tab in private browsing window"
      );
      await browser.windows.remove(win.id);

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
