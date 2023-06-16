/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

loadTestSubscript("head_devtools.js");

const FILE_URL = Services.io.newFileURI(
  new FileUtils.File(getTestFilePath("file_dummy.html"))
).spec;

add_task(async function test_devtools_inspectedWindow_eval_in_file_url() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FILE_URL);

  async function devtools_page() {
    try {
      const [evalResult, errorResult] =
        await browser.devtools.inspectedWindow.eval("location.protocol");
      browser.test.assertEq(undefined, errorResult, "eval should not fail");
      browser.test.assertEq("file:", evalResult, "eval should succeed");
      browser.test.notifyPass("inspectedWindow-eval-file");
    } catch (err) {
      browser.test.fail(`Error: ${err} :: ${err.stack}`);
      browser.test.notifyFail("inspectedWindow-eval-file");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
         <script src="devtools_page.js"></script>
       </head>
      </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  await openToolboxForTab(tab);

  await extension.awaitFinish("inspectedWindow-eval-file");

  await closeToolboxForTab(tab);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
