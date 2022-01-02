"use strict";

add_task(async function test_popup_requestPermission_resolve() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
      },
      optional_permissions: ["<all_urls>"],
    },

    files: {
      "popup.html": `<meta charset="utf-8"><script src="popup.js"></script>`,
      "popup.js": async () => {
        const success = await new Promise(resolve => {
          browser.test.withHandlingUserInput(() => {
            resolve(
              browser.permissions.request({
                origins: ["<all_urls>"],
              })
            );
          });
        });
        browser.test.assertTrue(
          success,
          "browser.permissions.request promise resolves"
        );
        browser.test.sendMessage("done");
      },
    },
  });

  const requestPrompt = promisePopupNotificationShown(
    "addon-webext-permissions"
  ).then(panel => {
    panel.button.click();
  });
  await extension.startup();
  await clickBrowserAction(extension);
  await requestPrompt;
  await extension.awaitMessage("done");
  await extension.unload();
});
