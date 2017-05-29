/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* testSidebarBrowserStyle(sidebarAction, assertMessage) {
  function sidebarScript() {
    browser.test.onMessage.addListener((msgName, info, assertMessage) => {
      if (msgName !== "check-style") {
        browser.test.notifyFail("options-ui-browser_style");
      }

      let style = window.getComputedStyle(document.getElementById("button"));
      let buttonBackgroundColor = style.backgroundColor;
      let browserStyleBackgroundColor = "rgb(9, 150, 248)";
      if (!("browser_style" in info) || info.browser_style) {
        browser.test.assertEq(browserStyleBackgroundColor, buttonBackgroundColor, assertMessage);
      } else {
        browser.test.assertTrue(browserStyleBackgroundColor !== buttonBackgroundColor, assertMessage);
      }

      browser.test.notifyPass("sidebar-browser-style");
    });
    browser.test.sendMessage("sidebar-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "sidebar_action": sidebarAction,
    },
    useAddonManager: "temporary",

    files: {
      "panel.html": `
        <!DOCTYPE html>
        <html>
          <button id="button" name="button" class="default">Default</button>
          <script src="panel.js" type="text/javascript"></script>
        </html>`,
      "panel.js": sidebarScript,
    },
  });

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser);

  yield extension.startup();
  yield extension.awaitMessage("sidebar-ready");

  extension.sendMessage("check-style", sidebarAction, assertMessage);
  yield extension.awaitFinish("sidebar-browser-style");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
}

add_task(function* test_sidebar_without_setting_browser_style() {
  yield testSidebarBrowserStyle({
    "default_panel": "panel.html",
  }, "Expected correct style when browser_style is excluded");
});

add_task(function* test_sidebar_with_browser_style_set_to_true() {
  yield testSidebarBrowserStyle({
    "default_panel": "panel.html",
    "browser_style": true,
  }, "Expected correct style when browser_style is set to `true`");
});

add_task(function* test_sidebar_with_browser_style_set_to_false() {
  yield testSidebarBrowserStyle({
    "default_panel": "panel.html",
    "browser_style": false,
  }, "Expected no style when browser_style is set to `false`");
});
