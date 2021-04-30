/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_cross_docGroup_adoption() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/"],
          js: ["content-script.js"],
        },
      ],
    },

    files: {
      "current.html": "<html>data</html>",
      "content-script.js": function() {
        let iframe = document.createElement("iframe");
        iframe.src = browser.runtime.getURL("current.html");
        document.body.appendChild(iframe);

        iframe.addEventListener(
          "load",
          () => {
            let parser = new DOMParser();
            let bold = parser.parseFromString(
              "<b>NodeAdopted</b>",
              "text/html"
            );
            let doc = iframe.contentDocument;

            let node = document.adoptNode(bold.documentElement);
            doc.replaceChild(node, doc.documentElement);

            const expected =
              "<html><head></head><body><b>NodeAdopted</b></body></html>";
            browser.test.assertEq(expected, doc.documentElement.outerHTML);

            browser.test.notifyPass("nodeAdopted");
          },
          { once: true }
        );
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("nodeAdopted");
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
