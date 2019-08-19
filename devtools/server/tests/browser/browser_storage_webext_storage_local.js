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
    info(
      "Setting up and connecting Debugger Server and Client in main process"
    );
    initDebuggerServer();
    const transport = DebuggerServer.connectPipe();
    const client = new DebuggerClient(transport);
    await client.connect();

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
