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
      "permissions": ["tabs"],
      "options_ui": {
        "page": "options.html",
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

    aboutAddonsBrowser.addEventListener("DOMWillOpenModalDialog", function onModalDialog(event) {
      if (Cu.isCrossProcessWrapper(event.target)) {
        // This event fires in both the content and chrome processes. We
        // want to ignore the one in the content process.
        return;
      }

      aboutAddonsBrowser.removeEventListener("DOMWillOpenModalDialog", onModalDialog, true);
      // Wait for the next event tick to make sure the remaining part of the
      // testcase runs after the dialog gets opened.
      SimpleTest.executeSoon(resolve);
    }, true);
  });

  info("Wait the options_ui modal to be opened");
  await onceModalOpened;

  const optionsBrowser = gBrowser.selectedBrowser.contentDocument
                                 .getElementById("addon-options");

  let stack;

  // For remote extensions, the stack that contains the tabmodalprompt elements
  // is the parent of the extensions options_ui browser element, otherwise it would
  // be the parent of the currently selected tabbrowser's browser.
  if (optionsBrowser.isRemoteBrowser) {
    stack = optionsBrowser.parentNode;
  } else {
    stack = gBrowser.selectedBrowser.parentNode;
  }

  let dialogs = stack.querySelectorAll("tabmodalprompt");

  Assert.equal(dialogs.length, 1, "Expect a tab modal opened for the about addons tab");

  info("Close the tab modal prompt");
  dialogs[0].onButtonClick(0);

  await extension.awaitFinish("options-ui-modals");

  Assert.equal(stack.querySelectorAll("tabmodalprompt").length, 0,
               "Expect the tab modal to be closed");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await extension.unload();
});
