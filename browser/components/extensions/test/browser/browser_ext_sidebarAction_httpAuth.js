/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

add_task(async function sidebar_httpAuthPrompt() {
  let data = {
    manifest: {
      permissions: ["https://example.com/*"],
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    useAddonManager: "temporary",
    files: {
      "sidebar.html": `
        <!DOCTYPE html>
        <html>
        <head><meta charset="utf-8"/>
        <script src="sidebar.js"></script>
        </head>
        <body>
        A Test Sidebar
        </body></html>
      `,
      "sidebar.js": function () {
        fetch(
          "https://example.com/browser/browser/components/extensions/test/browser/authenticate.sjs?user=user&pass=pass",
          { credentials: "include" }
        ).then(response => {
          browser.test.sendMessage("fetchResult", response.ok);
        });
      },
    },
  };

  // Wait for the http auth prompt and close it with accept button.
  let promptPromise = PromptTestUtils.handleNextPrompt(
    SidebarUI.browser.contentWindow,
    {
      modalType: Services.prompt.MODAL_TYPE_WINDOW,
      promptType: "promptUserAndPass",
    },
    { buttonNumClick: 0, loginInput: "user", passwordInput: "pass" }
  );

  let extension = ExtensionTestUtils.loadExtension(data);
  await extension.startup();
  let fetchResultPromise = extension.awaitMessage("fetchResult");

  await promptPromise;
  ok(true, "Extension fetch should trigger auth prompt.");

  let responseOk = await fetchResultPromise;
  ok(responseOk, "Login should succeed.");

  await extension.unload();

  // Cleanup
  await new Promise(resolve =>
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
      resolve
    )
  );
});
