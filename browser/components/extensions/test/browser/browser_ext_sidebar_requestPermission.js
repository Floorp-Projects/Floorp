"use strict";

add_task(async function test_sidebar_requestPermission_resolve() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      sidebar_action: {
        default_panel: "panel.html",
        browser_style: false,
      },
      optional_permissions: ["tabs"],
    },
    useAddonManager: "temporary",
    files: {
      "panel.html": `<meta charset="utf-8"><script src="panel.js"></script>`,
      "panel.js": async () => {
        const success = await new Promise(resolve => {
          browser.test.withHandlingUserInput(() => {
            resolve(
              browser.permissions.request({
                permissions: ["tabs"],
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
  await requestPrompt;
  await extension.awaitMessage("done");
  await extension.unload();
});
