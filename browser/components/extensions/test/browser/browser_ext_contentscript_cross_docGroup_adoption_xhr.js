/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_cross_docGroup_adoption() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.content_web_accessible.enabled", true]],
  });

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
      web_accessible_resources: ["blank.html"],
    },

    files: {
      "blank.html": "<html>data</html>",
      "content-script.js": function() {
        let xhr = new XMLHttpRequest();
        xhr.responseType = "document";
        xhr.open("GET", browser.runtime.getURL("blank.html"));

        xhr.onload = function() {
          let doc = xhr.response;
          try {
            let node = doc.body.cloneNode(true);
            document.body.appendChild(node);
            browser.test.notifyPass("nodeAdopted");
          } catch (SecurityError) {
            browser.test.assertTrue(
              false,
              "The above node adoption should not fail"
            );
          }
        };
        xhr.send();
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("nodeAdopted");
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
