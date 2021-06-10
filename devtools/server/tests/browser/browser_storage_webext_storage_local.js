/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Pref remains in effect until test completes and is automatically cleared afterwards
add_task(async function set_enable_extensionStorage_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.storage.extensionStorage.enabled", true]],
  });
});

add_task(
  async function test_extensionStorage_disabled_for_non_extension_target() {
    if (isFissionEnabled()) {
      ok(
        true,
        "This test is not relevant when fission is enabled as we aren't using listStores"
      );
      // And we don't implement the EXTENSION_STORAGE Resource yet
      return;
    }

    info(
      "Setting up and connecting DevTools Server and Client in main process"
    );
    info("Opening a non-extension page in a tab");
    const target = await addTabTarget("data:text/html;charset=utf-8,");

    info("Getting all stores for the target process");
    const storageFront = await target.getFront("storage");
    const stores = await storageFront.listStores();

    ok(
      !("extensionStorage" in stores),
      "Should not have an extensionStorage store when non-extension process is targeted"
    );

    await target.destroy();
    gBrowser.removeCurrentTab();
  }
);
