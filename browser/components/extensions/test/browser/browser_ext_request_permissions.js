"use strict";

add_task(async function test_permissions_prompt() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      optional_permissions: ["history"],
    },
    background: () => {
      browser.tabs.create({
        url: browser.runtime.getURL("test.html"),
        active: false,
      });
    },
    files: {
      "test.html": `<!DOCTYPE html><script src="test.js"></script>`,
      "test.js": async () => {
        const result = await new Promise(resolve => {
          browser.test.withHandlingUserInput(() => {
            resolve(browser.permissions.request({ permissions: ["history"] }));
          });
        });

        browser.test.assertFalse(
          result,
          "permissions.request() from a hidden tab should be ignored"
        );

        browser.test.sendMessage("done");
      },
    },
  });
  await extension.startup();

  await extension.awaitMessage("done");

  // The extension tab is automatically closed upon unload.
  await extension.unload();
});
