/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* testOptionsBrowserStyle(optionsUI, expectedMessage) {
  function optionsScript() {
    browser.test.onMessage.addListener((msg, optionsUI, expectedMessage) => {
      if (msg !== "check-style") {
        browser.test.notifyFail("options-ui-browser_style");
      }

      let style = window.getComputedStyle(document.getElementById("button"));
      if (!("browser_style" in optionsUI) || optionsUI.browser_style) {
        browser.test.assertEq("rgb(9, 150, 248)", style.backgroundColor, expectedMessage);
      } else {
        browser.test.assertEq("rgb(240, 240, 240)", style.backgroundColor, expectedMessage);
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

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser);

  yield extension.startup();
  yield extension.awaitMessage("options-ui-ready");

  extension.sendMessage("check-style", optionsUI, expectedMessage);
  yield extension.awaitFinish("options-ui-browser_style");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
}

add_task(function* test_options_without_setting_browser_style() {
  yield testOptionsBrowserStyle({
    "page": "options.html",
  }, "Expected correct style when browser_style is excluded");
});

add_task(function* test_options_with_browser_style_set_to_true() {
  yield testOptionsBrowserStyle({
    "page": "options.html",
    "browser_style": true,
  }, "Expected correct style when browser_style is set to `true`");
});

add_task(function* test_options_with_browser_style_set_to_false() {
  yield testOptionsBrowserStyle({
    "page": "options.html",
    "browser_style": false,
  }, "Expected no style when browser_style is set to `false`");
});
