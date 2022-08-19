/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

let win;

add_setup(async function() {
  // Only load a new window with the unified extensions feature enabled once to
  // speed up the execution of this test file.
  win = await promiseEnableUnifiedExtensions();

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

const verifyPermissionsPrompt = async (win, expectedAnchorID) => {
  const ext = ExtensionTestUtils.loadExtension({
    manifest: {
      optional_permissions: ["history"],
    },

    background: async () => {
      await browser.tabs.create({
        url: browser.runtime.getURL("content.html"),
        active: true,
      });
    },

    files: {
      "content.html": `<!DOCTYPE html><script src="content.js"></script>`,
      "content.js": async () => {
        browser.test.onMessage.addListener(async msg => {
          browser.test.assertEq(
            msg,
            "grant-permission",
            "expected message to grant permission"
          );

          const granted = await new Promise(resolve => {
            browser.test.withHandlingUserInput(() => {
              resolve(
                browser.permissions.request({ permissions: ["history"] })
              );
            });
          });
          browser.test.assertTrue(granted, "permission request succeeded");

          browser.test.sendMessage("ok");
        });

        browser.test.sendMessage("ready");
      },
    },
  });

  await BrowserTestUtils.withNewTab({ gBrowser: win.gBrowser }, async () => {
    await ext.startup();
    await ext.awaitMessage("ready");

    const popupPromise = promisePopupNotificationShown(
      "addon-webext-permissions",
      win
    );
    ext.sendMessage("grant-permission");
    const panel = await popupPromise;
    const notification = win.PopupNotifications.getNotification(
      "addon-webext-permissions"
    );
    ok(notification, "expected notification");
    is(
      // We access the parent element because the anchor is on the icon, not on
      // the unified extensions button itself.
      notification.anchorElement.id ||
        notification.anchorElement.parentElement.id,
      expectedAnchorID,
      "expected the right anchor ID"
    );

    panel.button.click();
    await ext.awaitMessage("ok");

    await ext.unload();
  });
};

add_task(async function test_permissions_prompt_with_pref_enabled() {
  await verifyPermissionsPrompt(win, "unified-extensions-button");
});

add_task(async function test_permissions_prompt_with_pref_disabled() {
  const anotherWindow = await promiseDisableUnifiedExtensions();

  await verifyPermissionsPrompt(anotherWindow, "addons-notification-icon");

  await BrowserTestUtils.closeWindow(anotherWindow);
});
