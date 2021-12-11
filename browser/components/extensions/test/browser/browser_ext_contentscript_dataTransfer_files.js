/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const TEST_ORIGIN = "http://mochi.test:8888";
const TEST_BASEURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  TEST_ORIGIN
);

const TEST_URL = `${TEST_BASEURL}file_dataTransfer_files.html`;

// This test ensure that we don't cache the DataTransfer files instances when
// they are being accessed by an extension content or user script (regression
// test related to Bug 1707214).
add_task(async function test_contentAndUserScripts_dataTransfer_files() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://mochi.test/"],
      user_scripts: {},
    },

    background: async function() {
      await browser.contentScripts.register({
        js: [{ file: "content_script.js" }],
        matches: ["http://mochi.test/*"],
        runAt: "document_start",
      });

      await browser.userScripts.register({
        js: [{ file: "user_script.js" }],
        matches: ["http://mochi.test/*"],
        runAt: "document_start",
      });

      browser.test.sendMessage("scripts-registered");
    },

    files: {
      "content_script.js": function() {
        document.addEventListener(
          "drop",
          function(e) {
            const files = e.dataTransfer.files || [];
            document.querySelector("#result-content-script").textContent =
              files[0]?.name;
          },
          { once: true, capture: true }
        );

        // Export a function that will be called by the drop event listener subscribed
        // by the test page itself, which is the last one to be registered and then
        // executed. This function retrieve the test results and send them to be
        // asserted for the expected filenames.
        this.exportFunction(
          () => {
            const results = {
              contentScript: document.querySelector("#result-content-script")
                .textContent,
              userScript: document.querySelector("#result-user-script")
                .textContent,
              pageScript: document.querySelector("#result-page-script")
                .textContent,
            };
            browser.test.sendMessage("test-done", results);
          },
          window,
          { defineAs: "testDone" }
        );
      },
      "user_script.js": function() {
        document.addEventListener(
          "drop",
          function(e) {
            const files = e.dataTransfer.files || [];
            document.querySelector("#result-user-script").textContent =
              files[0]?.name;
          },
          { once: true, capture: true }
        );
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("scripts-registered");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  const results = await extension.awaitMessage("test-done");
  const expectedFilename = "testfile.html";
  Assert.deepEqual(
    results,
    {
      contentScript: expectedFilename,
      userScript: expectedFilename,
      pageScript: expectedFilename,
    },
    "Got the expected drag and drop filenames"
  );

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
