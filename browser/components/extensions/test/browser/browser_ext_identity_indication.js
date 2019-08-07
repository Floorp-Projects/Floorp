/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function confirmDefaults() {
  let identityIconURL = getComputedStyle(
    document.getElementById("identity-icon")
  ).listStyleImage;
  is(
    identityIconURL,
    'url("chrome://browser/skin/search-glass.svg")',
    "Identity icon should be the search icon"
  );

  let label = document.getElementById("identity-icon-label");
  ok(
    BrowserTestUtils.is_hidden(label),
    "No label should be used before the extension is started"
  );
}

function confirmExtensionPage() {
  let identityIcon = getComputedStyle(document.getElementById("identity-icon"))
    .listStyleImage;
  is(
    identityIcon,
    'url("chrome://mozapps/skin/extensions/extensionGeneric-16.svg")',
    "Identity icon should be the default extension icon"
  );

  let identityIconEl = document.getElementById("identity-icon");
  let tooltip = identityIconEl.tooltipText;
  is(
    tooltip,
    "Loaded by extension: Test Extension",
    "The correct tooltip should be used"
  );

  let label = document.getElementById("identity-icon-label");
  is(
    label.value,
    "Extension (Test Extension)",
    "The correct label should be used"
  );
  ok(BrowserTestUtils.is_visible(label), "No label should be visible");
}

add_task(async function testIdentityIndication() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("url", browser.extension.getURL("icon.png"));
    },
    manifest: {
      name: "Test Extension",
    },
    files: {
      "icon.png": "",
    },
  });

  await extension.startup();

  confirmDefaults();

  let url = await extension.awaitMessage("url");
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function() {
    confirmExtensionPage();
  });

  await extension.unload();

  confirmDefaults();
});

add_task(async function testIdentityIndicationNewTab() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("url", browser.extension.getURL("newtab.html"));
    },
    manifest: {
      name: "Test Extension",
      applications: {
        gecko: {
          id: "@newtab",
        },
      },
      chrome_url_overrides: {
        newtab: "newtab.html",
      },
    },
    files: {
      "newtab.html": "<h1>New tab!</h1>",
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  confirmDefaults();

  let url = await extension.awaitMessage("url");
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function() {
    confirmExtensionPage();
    is(gURLBar.value, "", "The URL bar is blank");
  });

  await extension.unload();

  confirmDefaults();
});
