/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

async function waitForExtensionModalPrompt(extension) {
  const dialog = await PromptTestUtils.waitForPrompt(gBrowser.selectedBrowser, {
    modalType: Ci.nsIPrompt.MODAL_TYPE_CONTENT,
  });
  ok(dialog, "Got an active modal prompt dialog as expected");
  Assert.equal(
    dialog?.args.promptPrincipal.addonId,
    extension.id,
    "Got a prompt associated to the expected extension id"
  );

  const promptTitle = dialog?.ui.infoTitle.textContent;
  ok(
    /The page at TestExtName says:/.test(promptTitle),
    `Got the expect title on the modal prompt dialog: "${promptTitle}"`
  );

  return {
    async closeModalPrompt() {
      info("Close the tab modal prompt");
      await PromptTestUtils.handlePrompt(dialog);
    },
    assertClosedModalPrompt() {
      ok(
        !dialog.args.promptActive,
        "modal prompt dialog has been closed as expected"
      );
    },
  };
}

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
      name: "TestExtName",
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

  const testPromptPromise = waitForExtensionModalPrompt(extension);
  await extension.startup();

  info("Wait the options_ui modal to be opened");
  const testModalPrompt = await testPromptPromise;

  testModalPrompt.closeModalPrompt();
  await extension.awaitFinish("options-ui-modals");
  testModalPrompt.assertClosedModalPrompt();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await extension.unload();
});
