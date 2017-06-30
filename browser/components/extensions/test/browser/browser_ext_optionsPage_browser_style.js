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

      let browserStyle = !("browser_style" in optionsUI) || optionsUI.browser_style;

      function verifyButton(buttonElement, expected) {
        let buttonStyle = window.getComputedStyle(buttonElement);
        let buttonBackgroundColor = buttonStyle.backgroundColor;
        if (browserStyle && expected.hasBrowserStyleClass) {
          browser.test.assertEq("rgb(9, 150, 248)", buttonBackgroundColor, assertMessage);
        } else {
          browser.test.assertTrue(buttonBackgroundColor !== "rgb(9, 150, 248)", assertMessage);
        }
      }

      function verifyCheckboxOrRadio(type, element, expected) {
        let style = window.getComputedStyle(element);
        if (browserStyle && expected.hasBrowserStyleClass) {
          browser.test.assertEq("none", style.display, `Expected ${type} item to be hidden`);
        } else {
          browser.test.assertTrue(style.display != "none", `Expected ${type} item to be visible`);
        }
      }

      let normalButton = document.getElementById("normalButton");
      let browserStyleButton = document.getElementById("browserStyleButton");
      verifyButton(normalButton, {hasBrowserStyleClass: false});
      verifyButton(browserStyleButton, {hasBrowserStyleClass: true});

      let normalCheckbox1 = document.getElementById("normalCheckbox1");
      let normalCheckbox2 = document.getElementById("normalCheckbox2");
      let browserStyleCheckbox = document.getElementById("browserStyleCheckbox");
      verifyCheckboxOrRadio("checkbox", normalCheckbox1, {hasBrowserStyleClass: false});
      verifyCheckboxOrRadio("checkbox", normalCheckbox2, {hasBrowserStyleClass: false});
      verifyCheckboxOrRadio("checkbox", browserStyleCheckbox, {hasBrowserStyleClass: true});

      let normalRadio1 = document.getElementById("normalRadio1");
      let normalRadio2 = document.getElementById("normalRadio2");
      let browserStyleRadio = document.getElementById("browserStyleRadio");
      verifyCheckboxOrRadio("radio", normalRadio1, {hasBrowserStyleClass: false});
      verifyCheckboxOrRadio("radio", normalRadio2, {hasBrowserStyleClass: false});
      verifyCheckboxOrRadio("radio", browserStyleRadio, {hasBrowserStyleClass: true});

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
          <button id="normalButton" name="button" class="default">Default</button>
          <button id="browserStyleButton" name="button" class="browser-style default">Default</button>

          <input id="normalCheckbox1" type="checkbox"/>
          <input id="normalCheckbox2" type="checkbox"/><label>Checkbox</label>
          <div class="browser-style">
            <input id="browserStyleCheckbox" type="checkbox"><label for="browserStyleCheckbox">Checkbox</label>
          </div>

          <input id="normalRadio1" type="radio"/>
          <input id="normalRadio2" type="radio"/><label>Radio</label>
          <div class="browser-style">
            <input id="browserStyleRadio" checked="" type="radio"><label for="browserStyleRadio">Radio</label>
          </div>

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
