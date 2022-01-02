/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const FILE_URL = Services.io.newFileURI(
  new FileUtils.File(getTestFilePath("file_dummy.html"))
).spec;

async function testTabsCreateInvalidURL(tabsCreateURL) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: function() {
      browser.test.sendMessage("ready");
      browser.test.onMessage.addListener((msg, tabsCreateURL) => {
        browser.tabs.create({ url: tabsCreateURL }, tab => {
          browser.test.assertEq(
            undefined,
            tab,
            "on error tab should be undefined"
          );
          browser.test.assertTrue(
            /Illegal URL/.test(browser.runtime.lastError.message),
            "runtime.lastError should report the expected error message"
          );

          // Remove the opened tab is any.
          if (tab) {
            browser.tabs.remove(tab.id);
          }
          browser.test.sendMessage("done");
        });
      });
    },
  });

  await extension.startup();

  await extension.awaitMessage("ready");

  info(`test tab.create on invalid URL "${tabsCreateURL}"`);

  extension.sendMessage("start", tabsCreateURL);
  await extension.awaitMessage("done");

  await extension.unload();
}

add_task(async function() {
  info("Start testing tabs.create on invalid URLs");

  let dataURLPage = `data:text/html,
    <!DOCTYPE html>
    <html>
      <head>
        <meta charset="utf-8">
      </head>
      <body>
        <h1>data url page</h1>
      </body>
    </html>`;

  let testCases = [
    { tabsCreateURL: "about:addons" },
    {
      tabsCreateURL: "javascript:console.log('tabs.update execute javascript')",
    },
    { tabsCreateURL: dataURLPage },
    { tabsCreateURL: FILE_URL },
  ];

  for (let { tabsCreateURL } of testCases) {
    await testTabsCreateInvalidURL(tabsCreateURL);
  }

  info("done");
});
