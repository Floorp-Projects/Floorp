/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

// This test verifies that cached css are not being used across addon updates or addon reloads,
// See Bug 1746841.
add_task(async function test_cached_resources_across_addon_updates() {
  const ADDON_ID = "test-cached-resources@test";
  const background = () => browser.test.sendMessage("bg-ready");
  const manifest = {
    version: "1",
    browser_specific_settings: { gecko: { id: ADDON_ID } },
    browser_action: {
      default_popup: "popup.html",
    },
  };
  const files = {
    "popup.html": `<!DOCTYPE html>
      <html>
        <head>
          <link rel="stylesheet" href="popup.css">
          <script defer src="popup.js"></script>
        </head>
        <body>
          browserAction popup
        </body>
      </html>
    `,
    "popup.css": "body { background-color: rgb(255, 0, 0); }",
    "popup.js": function() {
      window.onload = () => {
        browser.test.sendMessage(
          "popup-bgcolor",
          window.getComputedStyle(document.body).backgroundColor
        );
      };
    },
  };

  const extV1 = ExtensionTestUtils.loadExtension({
    background,
    manifest,
    files,
  });

  await extV1.startup();
  await extV1.awaitMessage("bg-ready");

  clickBrowserAction(extV1);
  const bgcolorV1 = await extV1.awaitMessage("popup-bgcolor");
  is(
    bgcolorV1,
    "rgb(255, 0, 0)",
    "Got the expected background color for the initial extension version"
  );
  await closeBrowserAction(extV1);
  await extV1.unload();

  // Faking an addon update by installing an updated test extension with the same extension id.
  const extV2 = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      ...manifest,
      version: "2",
    },
    files: {
      ...files,
      "popup.css": "body { background-color: rgb(0, 255, 0); }",
    },
  });

  await extV2.startup();
  await extV2.awaitMessage("bg-ready");

  clickBrowserAction(extV2);
  const bgcolorV2 = await extV2.awaitMessage("popup-bgcolor");
  is(
    bgcolorV2,
    "rgb(0, 255, 0)",
    "Got the expected background color for the initial extension version"
  );
  await closeBrowserAction(extV1);

  await extV2.unload();
});
