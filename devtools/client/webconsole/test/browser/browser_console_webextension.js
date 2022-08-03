/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that messages from WebExtension are logged in the Browser Console.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html?" +
  Date.now();

add_task(async function() {
  await pushPref("devtools.browserconsole.contentMessages", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  await addTab(TEST_URI);

  info("Check with devtools.browsertoolbox.fission set to false");
  await pushPref("devtools.browsertoolbox.fission", false);
  await testWebExtensionMessages(false);
  await testWebExtensionMessages(true);

  // ⚠️ When the pref is disabled, we only clear the cache for messages in the parent process,
  // so all the messages that were forwarded from the content process won't be cleared.
  // so here we open a multiprocess browser console and clear it, which will clear all the
  // caches.
  await pushPref("devtools.browsertoolbox.fission", true);
  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  await clearOutput(hud);
  await safeCloseBrowserConsole();

  info("Check with devtools.browsertoolbox.fission set to true");
  await testWebExtensionMessages(false);
  await testWebExtensionMessages(true);
});

async function testWebExtensionMessages(
  createWebExtensionBeforeOpeningBrowserConsole = false
) {
  let extension;
  if (createWebExtensionBeforeOpeningBrowserConsole) {
    extension = await loadExtension();
  }
  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  if (!createWebExtensionBeforeOpeningBrowserConsole) {
    extension = await loadExtension();
  }

  // TODO: Re-enable this (See Bug 1699050).
  /*
  // Trigger the messages logged when opening the popup.
  const { AppUiTestDelegate } = ChromeUtils.import(
    "resource://testing-common/AppUiTestDelegate.jsm"
  );
  const onPopupReady = extension.awaitMessage(`popup-ready`);
  await AppUiTestDelegate.clickBrowserAction(window, extension.id);
  // Ensure the popup script ran before going further
  AppUiTestDelegate.awaitExtensionPanel(window, extension.id);
  await onPopupReady;
  */

  // Wait enough so any duplicated message would have the time to be rendered
  await wait(1000);

  // When the pref is disabled, we're getting Console API messages via the parent process,
  // which were forwarded from the content process. But this forwarding mechanism does not
  // handle cached content messages (i.e. messages logged by the extension in the content
  // process, before the console is opened)
  if (
    !createWebExtensionBeforeOpeningBrowserConsole ||
    Services.prefs.getBoolPref("devtools.browsertoolbox.fission", false)
  ) {
    await checkUniqueMessageExists(
      hud,
      "content console API message",
      ".console-api"
    );
    await checkUniqueMessageExists(
      hud,
      "background console API message",
      ".console-api"
    );
  }

  await checkUniqueMessageExists(hud, "content error", ".error");
  await checkUniqueMessageExists(hud, "background error", ".error");

  // TODO: Re-enable those checks (See Bug 1699050).
  // await checkUniqueMessageExists(hud, "popup console API message", ".console-api");
  // await checkUniqueMessageExists(hud, "popup error", ".error");

  await clearOutput(hud);

  info("Close the Browser Console");
  await safeCloseBrowserConsole();

  await extension.unload();
}

async function loadExtension() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: { scripts: ["background.js"] },

      browser_action: {
        default_popup: "popup.html",
      },

      content_scripts: [
        {
          matches: [TEST_URI],
          js: ["content-script.js"],
        },
      ],
    },
    useAddonManager: "temporary",

    files: {
      "background.js": function() {
        console.log("background console API message");
        throw new Error("background error");
      },

      "popup.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>Popup</body>
          <script src="popup.js"></script>
        </html>`,

      "popup.js": function() {
        console.log("popup console API message");
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(() => {
          throw new Error("popup error");
        }, 5);

        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(() => {
          // eslint-disable-next-line no-undef
          browser.test.sendMessage(`popup-ready`);
        }, 10);
      },

      "content-script.js": function() {
        console.log("content console API message");
        throw new Error("content error");
      },
    },
  });
  await extension.startup();
  return extension;
}
