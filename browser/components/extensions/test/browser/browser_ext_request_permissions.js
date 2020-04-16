"use strict";

// This test case verifies that `permissions.request()` resolves in the
// expected order.
add_task(async function test_permissions_prompt() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      optional_permissions: ["history", "bookmarks"],
    },
    background: async () => {
      let hiddenTab = await browser.tabs.create({
        url: browser.runtime.getURL("hidden.html"),
        active: false,
      });

      await browser.tabs.create({
        url: browser.runtime.getURL("active.html"),
        active: true,
      });

      browser.test.onMessage.addListener(async msg => {
        if (msg === "activate-hiddenTab") {
          await browser.tabs.update(hiddenTab.id, { active: true });

          browser.test.sendMessage("activate-hiddenTab-ok");
        }
      });
    },
    files: {
      "active.html": `<!DOCTYPE html><script src="active.js"></script>`,
      "active.js": async () => {
        browser.test.onMessage.addListener(async msg => {
          if (msg === "request-perms-activeTab") {
            let granted = await new Promise(resolve => {
              browser.test.withHandlingUserInput(() => {
                resolve(
                  browser.permissions.request({ permissions: ["history"] })
                );
              });
            });
            browser.test.assertTrue(granted, "permission request succeeded");

            browser.test.sendMessage("request-perms-activeTab-ok");
          }
        });

        browser.test.sendMessage("activeTab-ready");
      },
      "hidden.html": `<!DOCTYPE html><script src="hidden.js"></script>`,
      "hidden.js": async () => {
        let resolved = false;

        browser.test.onMessage.addListener(async msg => {
          if (msg === "request-perms-hiddenTab") {
            let granted = await new Promise(resolve => {
              browser.test.withHandlingUserInput(() => {
                resolve(
                  browser.permissions.request({ permissions: ["bookmarks"] })
                );
              });
            });
            browser.test.assertTrue(granted, "permission request succeeded");

            resolved = true;

            browser.test.sendMessage("request-perms-hiddenTab-ok");
          } else if (msg === "hiddenTab-read-state") {
            browser.test.sendMessage("hiddenTab-state-value", resolved);
          }
        });

        browser.test.sendMessage("hiddenTab-ready");
      },
    },
  });
  await extension.startup();

  await extension.awaitMessage("activeTab-ready");
  await extension.awaitMessage("hiddenTab-ready");

  // Call request() on a hidden window.
  extension.sendMessage("request-perms-hiddenTab");

  let requestPromptForActiveTab = promisePopupNotificationShown(
    "addon-webext-permissions"
  ).then(panel => {
    panel.button.click();
  });

  // Call request() in the current window.
  extension.sendMessage("request-perms-activeTab");
  await requestPromptForActiveTab;
  await extension.awaitMessage("request-perms-activeTab-ok");

  // Check that initial request() is still pending.
  extension.sendMessage("hiddenTab-read-state");
  ok(
    !(await extension.awaitMessage("hiddenTab-state-value")),
    "initial request is pending"
  );

  let requestPromptForHiddenTab = promisePopupNotificationShown(
    "addon-webext-permissions"
  ).then(panel => {
    panel.button.click();
  });

  extension.sendMessage("activate-hiddenTab");
  await extension.awaitMessage("activate-hiddenTab-ok");
  await requestPromptForHiddenTab;
  await extension.awaitMessage("request-perms-hiddenTab-ok");

  extension.sendMessage("hiddenTab-read-state");
  ok(
    await extension.awaitMessage("hiddenTab-state-value"),
    "initial request is resolved"
  );

  // The extension tabs are automatically closed upon unload.
  await extension.unload();
});
