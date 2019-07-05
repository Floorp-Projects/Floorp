/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_sidebar_click_isAppTab_behavior() {
  function sidebarScript() {
    browser.tabs.onUpdated.addListener(function onUpdated(
      tabId,
      changeInfo,
      tab
    ) {
      if (
        changeInfo.status == "complete" &&
        tab.url == "http://mochi.test:8888/"
      ) {
        browser.tabs.remove(tab.id);
        browser.test.notifyPass("sidebar-click");
      }
    });
    window.addEventListener(
      "load",
      () => {
        browser.test.sendMessage("sidebar-ready");
      },
      { once: true }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      sidebar_action: {
        default_panel: "panel.html",
        browser_style: false,
      },
      permissions: ["tabs"],
    },
    useAddonManager: "temporary",

    files: {
      "panel.html": `
        <!DOCTYPE html>
        <html>
          <head>
          <meta charset="utf-8">
          </head>
          <script src="panel.js" type="text/javascript"></script>
          <a id="testlink" href="http://mochi.test:8888">Bugzilla</a>
        </html>`,
      "panel.js": sidebarScript,
    },
  });

  await extension.startup();
  await extension.awaitMessage("sidebar-ready");

  // This test fails if docShell.isAppTab has not been set to true.
  let content = SidebarUI.browser.contentWindow;
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#testlink",
    {},
    content.gBrowser.selectedBrowser
  );
  await extension.awaitFinish("sidebar-click");

  await extension.unload();
});
