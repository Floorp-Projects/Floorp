"use strict";

add_task(async function () {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: "commands@mochi.test" } },
      commands: {
        foo: {
          suggested_key: {
            default: "Ctrl+Shift+V",
          },
          description: "The foo command",
        },
      },
    },
    background: async function () {
      const { commands } = browser.runtime.getManifest();

      const originalFoo = commands.foo;

      let resolver = {};
      resolver.promise = new Promise(resolve => (resolver.resolve = resolve));

      browser.commands.onChanged.addListener(update => {
        browser.test.assertDeepEq(
          update,
          {
            name: "foo",
            newShortcut: "Ctrl+Shift+L",
            oldShortcut: originalFoo.suggested_key.default,
          },
          `The name should match what was provided in the manifest.
           The new shortcut should match what was provided in the update.
           The old shortcut should match what was provided in the manifest
          `
        );
        browser.test.assertFalse(
          resolver.hasResolvedAlready,
          `resolver was not resolved yet`
        );
        resolver.resolve();
        resolver.hasResolvedAlready = true;
      });

      await browser.commands.update({ name: "foo", shortcut: "Ctrl+Shift+L" });
      // We're checking that nothing emits when
      // the new shortcut is identical to the old one
      await browser.commands.update({ name: "foo", shortcut: "Ctrl+Shift+L" });

      await resolver.promise;

      browser.test.notifyPass("commands");
    },
  });
  await extension.startup();
  await extension.awaitFinish("commands");
  await extension.unload();
});
