/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_tab_options_modals() {
  function backgroundScript() {
    browser.runtime.openOptionsPage();
  }

  function optionsScript() {
    try {
      alert("WebExtensions OptionsUI Page Modal");

      browser.test.notifyPass("options-ui-modals");
    } catch (error) {
      browser.test.log(`Error: ${error} :: ${error.stack}`);
      browser.test.notifyFail("options-ui-modals");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      permissions: ["tabs"],
      options_ui: {
        page: "options.html",
      },
    },
    files: {
      "options.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="options.js" type="text/javascript"></script>
          </head>
        </html>`,
      "options.js": optionsScript,
    },
    background: backgroundScript,
  });

  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:addons");

  await extension.startup();

  const onceModalOpened = new Promise(resolve => {
    const aboutAddonsBrowser = gBrowser.selectedBrowser;

    aboutAddonsBrowser.addEventListener(
      "DOMWillOpenModalDialog",
      function onModalDialog(event) {
        if (Cu.isCrossProcessWrapper(event.target)) {
          // This event fires in both the content and chrome processes. We
          // want to ignore the one in the content process.
          return;
        }

        aboutAddonsBrowser.removeEventListener(
          "DOMWillOpenModalDialog",
          onModalDialog,
          true
        );
        // Wait for the next event tick to make sure the remaining part of the
        // testcase runs after the dialog gets opened.
        SimpleTest.executeSoon(resolve);
      },
      true
    );
  });

  info("Wait the options_ui modal to be opened");
  await onceModalOpened;

  const optionsBrowser = getInlineOptionsBrowser(gBrowser.selectedBrowser);

  // The stack that contains the tabmodalprompt elements is the parent of
  // the extensions options_ui browser element.
  let stack = optionsBrowser.parentNode;

  let dialogs = stack.querySelectorAll("tabmodalprompt");
  Assert.equal(
    dialogs.length,
    1,
    "Expect a tab modal opened for the about addons tab"
  );

  // Verify that the expected stylesheets have been applied on the
  // tabmodalprompt element (See Bug 1550529).
  const tabmodalStyle = dialogs[0].ownerGlobal.getComputedStyle(dialogs[0]);
  is(
    tabmodalStyle["background-color"],
    "rgba(26, 26, 26, 0.5)",
    "Got the expected styles applied to the tabmodalprompt"
  );

  info("Close the tab modal prompt");
  dialogs[0].querySelector(".tabmodalprompt-button0").click();

  await extension.awaitFinish("options-ui-modals");

  Assert.equal(
    stack.querySelectorAll("tabmodalprompt").length,
    0,
    "Expect the tab modal to be closed"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await extension.unload();
});
