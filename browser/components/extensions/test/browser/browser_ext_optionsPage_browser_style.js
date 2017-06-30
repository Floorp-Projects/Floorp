/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

async function testOptionsBrowserStyle(optionsUI, assertMessage) {
  function optionsScript() {
    browser.test.onMessage.addListener((msgName, optionsUI, assertMessage) => {
      if (msgName !== "check-style") {
        browser.test.notifyFail("options-ui-browser_style");
      }

      let style = window.getComputedStyle(document.getElementById("button"));
      let buttonBackgroundColor = style.backgroundColor;
      let browserStyleBackgroundColor = "rgb(9, 150, 248)";
      if (!("browser_style" in optionsUI) || optionsUI.browser_style) {
        browser.test.assertEq(browserStyleBackgroundColor, buttonBackgroundColor, assertMessage);
      } else {
        browser.test.assertTrue(browserStyleBackgroundColor !== buttonBackgroundColor, assertMessage);
      }

      browser.test.notifyPass("options-ui-browser_style");
    });
    browser.test.sendMessage("options-ui-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      "permissions": ["tabs"],
      "options_ui": optionsUI,
    },
    files: {
      "options.html": `
        <!DOCTYPE html>
        <html>
          <button id="button" name="button" class="default">Default</button>
          <script src="options.js" type="text/javascript"></script>
        </html>`,
      "options.js": optionsScript,
    },
    background() {
      browser.runtime.openOptionsPage();
    },
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await extension.startup();
  await extension.awaitMessage("options-ui-ready");

  extension.sendMessage("check-style", optionsUI, assertMessage);
  await extension.awaitFinish("options-ui-browser_style");

  await BrowserTestUtils.removeTab(tab);

  await extension.unload();
}

add_task(async function test_options_without_setting_browser_style() {
  await testOptionsBrowserStyle({
    "page": "options.html",
  }, "Expected correct style when browser_style is excluded");
});

add_task(async function test_options_with_browser_style_set_to_true() {
  await testOptionsBrowserStyle({
    "page": "options.html",
    "browser_style": true,
  }, "Expected correct style when browser_style is set to `true`");
});

add_task(async function test_options_with_browser_style_set_to_false() {
  await testOptionsBrowserStyle({
    "page": "options.html",
    "browser_style": false,
  }, "Expected no style when browser_style is set to `false`");
});
