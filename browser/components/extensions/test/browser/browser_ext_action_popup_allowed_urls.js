/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
});

add_task(async function test_actions_setPopup_allowed_urls() {
  const otherExtension = ExtensionTestUtils.loadExtension({});
  const extensionDefinition = {
    background() {
      browser.test.onMessage.addListener(async (msg, ...args) => {
        if (msg === "set-popup") {
          const apiNs = args[0];
          const popupOptions = args[1];
          if (apiNs === "pageAction") {
            popupOptions.tabId = (
              await browser.tabs.query({ active: true })
            )[0].id;
          }

          let error;
          try {
            await browser[apiNs].setPopup(popupOptions);
          } catch (err) {
            error = err;
          }
          browser.test.sendMessage("set-popup:done", {
            error: error && String(error),
          });
          return;
        }

        browser.test.fail(`Unexpected test message: ${msg}`);
      });
    },
  };

  await otherExtension.startup();

  const testCases = [
    // https urls are disallowed on mv3 but currently allowed on mv2.
    [
      "action",
      "https://example.com",
      {
        manifest_version: 3,
        action: {},
      },
      {
        disallowed: true,
        errorMessage: "Access denied for URL https://example.com",
      },
    ],

    [
      "pageAction",
      "https://example.com",
      {
        manifest_version: 3,
        page_action: {},
      },
      {
        disallowed: true,
        errorMessage: "Access denied for URL https://example.com",
      },
    ],

    [
      "browserAction",
      "https://example.com",
      {
        manifest_version: 2,
        browser_action: {},
      },
      {
        disallowed: false,
      },
    ],

    [
      "pageAction",
      "https://example.com",
      {
        manifest_version: 2,
        page_action: {},
      },
      {
        disallowed: false,
      },
    ],

    // absolute moz-extension url from same extension expected to be allowed in MV3 and MV2.

    [
      "action",
      extension => `moz-extension://${extension.uuid}/page.html`,
      {
        manifest_version: 3,
        action: {},
      },
      {
        disallowed: false,
      },
    ],

    [
      "browserAction",
      extension => `moz-extension://${extension.uuid}/page.html`,
      {
        manifest_version: 2,
        browser_action: { default_popup: "popup.html" },
      },
      {
        disallowed: false,
      },
    ],

    [
      "pageAction",
      extension => `moz-extension://${extension.uuid}/page.html`,
      {
        manifest_version: 3,
        page_action: {},
      },
      {
        disallowed: false,
      },
    ],

    [
      "pageAction",
      extension => `moz-extension://${extension.uuid}/page.html`,
      {
        manifest_version: 2,
        page_action: {},
      },
      {
        disallowed: false,
      },
    ],

    // absolute moz-extension url from other extensions expected to be disallowed in MV3 and MV2.

    [
      "action",
      `moz-extension://${otherExtension.uuid}/page.html`,
      {
        manifest_version: 3,
        action: {},
      },
      {
        disallowed: true,
        errorMessage: `Access denied for URL moz-extension://${otherExtension.uuid}/page.html`,
      },
    ],

    [
      "browserAction",
      `moz-extension://${otherExtension.uuid}/page.html`,
      {
        manifest_version: 2,
        browser_action: {},
      },
      {
        disallowed: true,
        errorMessage: `Access denied for URL moz-extension://${otherExtension.uuid}/page.html`,
      },
    ],

    [
      "pageAction",
      `moz-extension://${otherExtension.uuid}/page.html`,
      {
        manifest_version: 3,
        page_action: {},
      },
      {
        disallowed: true,
        errorMessage: `Access denied for URL moz-extension://${otherExtension.uuid}/page.html`,
      },
    ],

    [
      "pageAction",
      `moz-extension://${otherExtension.uuid}/page.html`,
      {
        manifest_version: 2,
        page_action: {},
      },
      {
        disallowed: true,
        errorMessage: `Access denied for URL moz-extension://${otherExtension.uuid}/page.html`,
      },
    ],

    // Empty url should also be allowed (as it resets the popup url currently set).
    [
      "action",
      null,
      {
        manifest_version: 3,
        action: {},
      },
      {
        disallowed: false,
      },
    ],

    [
      "browserAction",
      null,
      {
        manifest_version: 2,
        browser_action: {},
      },
      {
        disallowed: false,
      },
    ],

    [
      "pageAction",
      null,
      {
        manifest_version: 3,
        page_action: {},
      },
      {
        disallowed: false,
      },
    ],

    [
      "pageAction",
      null,
      {
        manifest_version: 2,
        page_action: {},
      },
      {
        disallowed: false,
      },
    ],
  ];

  for (const [apiNs, popupUrl, manifest, expects] of testCases) {
    const extension = ExtensionTestUtils.loadExtension({
      ...extensionDefinition,
      manifest,
    });
    await extension.startup();

    const popup =
      typeof popupUrl === "function" ? popupUrl(extension) : popupUrl;

    info(
      `Testing ${apiNs}.setPopup({ popup: ${popup} }) on manifest_version ${manifest.manifest_version ??
        2}`
    );

    const popupOptions = { popup };
    extension.sendMessage("set-popup", apiNs, popupOptions);

    const { error } = await extension.awaitMessage("set-popup:done");
    if (expects.disallowed) {
      ok(
        error?.includes(expects.errorMessage),
        `Got expected error on url ${popup}: ${error}`
      );
    } else {
      is(error, undefined, `Expected url ${popup} to be allowed`);
    }
    await extension.unload();
  }

  await otherExtension.unload();
});
