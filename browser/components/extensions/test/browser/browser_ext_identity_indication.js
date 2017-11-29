/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function confirmDefaults() {
  let identityIconURL = getComputedStyle(document.getElementById("identity-icon")).listStyleImage;
  is(identityIconURL, "url(\"chrome://browser/skin/search-glass.svg\")", "Identity icon should be the search icon");

  let connectionIconURL = getComputedStyle(document.getElementById("connection-icon")).listStyleImage;
  is(connectionIconURL, "none", "Connection icon should not be displayed");

  let extensionIconURL = getComputedStyle(document.getElementById("extension-icon")).listStyleImage;
  is(extensionIconURL, "none", "Extension icon should not be displayed");

  let label = document.getElementById("identity-icon-label").value;
  is(label, "", "No label should be used before the extension is started");
}

function confirmExtensionPage() {
  let identityIcon = getComputedStyle(document.getElementById("identity-icon")).listStyleImage;
  is(identityIcon, "url(\"chrome://browser/skin/identity-icon.svg\")", "Identity icon  should be the default identity icon");

  let connectionIconURL = getComputedStyle(document.getElementById("connection-icon")).listStyleImage;
  is(connectionIconURL, "none", "Connection icon should not be displayed");

  let extensionIconEl = document.getElementById("extension-icon");
  let extensionIconURL = getComputedStyle(extensionIconEl).listStyleImage;
  is(extensionIconURL, "url(\"chrome://mozapps/skin/extensions/extensionGeneric-16.svg\")", "Extension icon should be the default extension icon");
  let tooltip = extensionIconEl.tooltipText;
  is(tooltip, "Loaded by extension: Test Extension", "The correct tooltip should be used");

  let label = document.getElementById("identity-icon-label").value;
  is(label, "Extension (Test Extension)", "The correct label should be used");
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
  await BrowserTestUtils.withNewTab({gBrowser, url}, async function() {
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
  await BrowserTestUtils.withNewTab({gBrowser, url}, async function() {
    confirmExtensionPage();
    is(gURLBar.value, "", "The URL bar is blank");
  });

  await extension.unload();

  confirmDefaults();
});
