/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function auto_approve_optional_permissions() {
  // Auto-approve optional permission requests, without UI.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextOptionalPermissionPrompts", false]],
  });
  // TODO: Consider an observer for "webextension-optional-permission-prompt"
  // once bug 1493396 is fixed.
});

add_task(async function overrideContext_permissions() {
  function sidebarJs() {
    // If the extension has the right permissions, calling
    // menus.overrideContext with one of the following should not throw.
    const CONTEXT_OPTIONS_TAB = { context: "tab", tabId: 1 };
    const CONTEXT_OPTIONS_BOOKMARK = { context: "bookmark", bookmarkId: "x" };

    const E_PERM_TAB = /The "tab" context requires the "tabs" permission/;
    const E_PERM_BOOKMARK = /The "bookmark" context requires the "bookmarks" permission/;

    function assertAllowed(contextOptions) {
      try {
        let result = browser.menus.overrideContext(contextOptions);
        browser.test.assertEq(
          undefined,
          result,
          `Allowed menu for context=${contextOptions.context}`
        );
      } catch (e) {
        browser.test.fail(
          `Unexpected error for context=${contextOptions.context}: ${e}`
        );
      }
    }

    function assertNotAllowed(contextOptions, expectedError) {
      browser.test.assertThrows(
        () => {
          browser.menus.overrideContext(contextOptions);
        },
        expectedError,
        `Expected error for context=${contextOptions.context}`
      );
    }

    async function requestPermissions(permissions) {
      try {
        let permPromise;
        window.withHandlingUserInputForPermissionRequestTest(() => {
          permPromise = browser.permissions.request(permissions);
        });
        browser.test.assertTrue(
          await permPromise,
          `Should have granted ${JSON.stringify(permissions)}`
        );
      } catch (e) {
        browser.test.fail(
          `Failed to use permissions.request(${JSON.stringify(
            permissions
          )}): ${e}`
        );
      }
    }

    // The menus.overrideContext method can only be called during a
    // "contextmenu" event. So we use a generator to run tests, and yield
    // before we call overrideContext after an asynchronous operation.
    let testGenerator = (async function*() {
      browser.test.assertEq(
        undefined,
        browser.menus.overrideContext,
        "menus.overrideContext requires the 'menus.overrideContext' permission"
      );
      await requestPermissions({ permissions: ["menus.overrideContext"] });
      yield;

      // context without required property.
      browser.test.assertThrows(
        () => {
          browser.menus.overrideContext({ context: "tab" });
        },
        /Property "tabId" is required for context "tab"/,
        "Required property for context tab"
      );
      browser.test.assertThrows(
        () => {
          browser.menus.overrideContext({ context: "bookmark" });
        },
        /Property "bookmarkId" is required for context "bookmark"/,
        "Required property for context bookmarks"
      );

      // context with too many properties.
      browser.test.assertThrows(
        () => {
          browser.menus.overrideContext({
            context: "bookmark",
            bookmarkId: "x",
            tabId: 1,
          });
        },
        /Property "tabId" can only be used with context "tab"/,
        "Invalid property for context bookmarks"
      );
      browser.test.assertThrows(
        () => {
          browser.menus.overrideContext({
            context: "bookmark",
            bookmarkId: "x",
            showDefaults: true,
          });
        },
        /Property "showDefaults" cannot be used with context "bookmark"/,
        "showDefaults cannot be used with context bookmark"
      );

      // context with right properties, but missing permissions.
      assertNotAllowed(CONTEXT_OPTIONS_BOOKMARK, E_PERM_BOOKMARK);
      assertNotAllowed(CONTEXT_OPTIONS_TAB, E_PERM_TAB);

      await requestPermissions({ permissions: ["bookmarks"] });
      browser.test.log("Active permissions: bookmarks");
      yield;

      assertAllowed(CONTEXT_OPTIONS_BOOKMARK);
      assertNotAllowed(CONTEXT_OPTIONS_TAB, E_PERM_TAB);

      await requestPermissions({ permissions: ["tabs"] });
      await browser.permissions.remove({ permissions: ["bookmarks"] });
      browser.test.log("Active permissions: tabs");
      yield;

      assertNotAllowed(CONTEXT_OPTIONS_BOOKMARK, E_PERM_BOOKMARK);
      assertAllowed(CONTEXT_OPTIONS_TAB);
      await browser.permissions.remove({ permissions: ["tabs"] });
      browser.test.log("Active permissions: none");
      yield;

      assertNotAllowed(CONTEXT_OPTIONS_TAB, E_PERM_TAB);

      await browser.permissions.remove({
        permissions: ["menus.overrideContext"],
      });
      browser.test.assertEq(
        undefined,
        browser.menus.overrideContext,
        "menus.overrideContext is unavailable after revoking the permission"
      );
    })();

    // eslint-disable-next-line mozilla/balanced-listeners
    document.addEventListener("contextmenu", async event => {
      event.preventDefault();
      try {
        let { done } = await testGenerator.next();
        browser.test.sendMessage("continue_test", !done);
      } catch (e) {
        browser.test.fail(`Unexpected error: ${e} :: ${e.stack}`);
        browser.test.sendMessage("continue_test", false);
      }
    });
    browser.test.sendMessage("sidebar_ready");
  }
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      permissions: ["menus"],
      optional_permissions: ["menus.overrideContext", "tabs", "bookmarks"],
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    files: {
      "sidebar.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <a href="http://example.com/">Link</a>
        <script src="sidebar.js"></script>
      `,
      "sidebar.js": sidebarJs,
    },
  });
  await extension.startup();
  await extension.awaitMessage("sidebar_ready");

  // permissions.request requires user input, export helper.
  await SpecialPowers.spawn(
    SidebarUI.browser.contentDocument.getElementById("webext-panels-browser"),
    [],
    () => {
      let { withHandlingUserInput } = ChromeUtils.import(
        "resource://gre/modules/ExtensionCommon.jsm",
        {}
      ).ExtensionCommon;
      Cu.exportFunction(
        fn => {
          return withHandlingUserInput(content, fn);
        },
        content,
        {
          defineAs: "withHandlingUserInputForPermissionRequestTest",
        }
      );
    }
  );

  do {
    info(`Going to trigger "contextmenu" event.`);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "a",
      { type: "contextmenu" },
      SidebarUI.browser.contentDocument.getElementById("webext-panels-browser")
    );
  } while (await extension.awaitMessage("continue_test"));

  await extension.unload();
});
