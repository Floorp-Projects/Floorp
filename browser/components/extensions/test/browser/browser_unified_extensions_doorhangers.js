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
    useAddonManager: "temporary",

    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "some search name",
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
      optional_permissions: ["history"],
    },

    background: () => {
      browser.test.onMessage.addListener(async msg => {
        if (msg !== "create-tab") {
          return;
        }

        await browser.tabs.create({
          url: browser.runtime.getURL("content.html"),
          active: true,
        });
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
    const defaultSearchPopupPromise = promisePopupNotificationShown(
      "addon-webext-defaultsearch",
      win
    );
    let [panel] = await Promise.all([defaultSearchPopupPromise, ext.startup()]);
    ok(panel, "expected panel");
    let notification = win.PopupNotifications.getNotification(
      "addon-webext-defaultsearch"
    );
    ok(notification, "expected notification");
    // We always want the defaultsearch popup to be anchored on the urlbar (the
    // ID below) because the post-install popup would be displayed on top of
    // this one otherwise, see Bug 1789407.
    is(
      notification?.anchorElement?.id,
      "addons-notification-icon",
      "expected the right anchor ID for the defaultsearch popup"
    );
    // Accept to override the search.
    panel.button.click();
    await TestUtils.topicObserved("webextension-defaultsearch-prompt-response");

    ext.sendMessage("create-tab");
    await ext.awaitMessage("ready");

    const popupPromise = promisePopupNotificationShown(
      "addon-webext-permissions",
      win
    );
    ext.sendMessage("grant-permission");
    panel = await popupPromise;
    ok(panel, "expected panel");
    notification = win.PopupNotifications.getNotification(
      "addon-webext-permissions"
    );
    ok(notification, "expected notification");
    is(
      // We access the parent element because the anchor is on the icon (inside
      // the button), not on the unified extensions button itself.
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
