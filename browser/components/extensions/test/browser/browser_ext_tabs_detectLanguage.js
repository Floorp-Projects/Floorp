/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testDetectLanguage() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background() {
      const BASE_PATH = "browser/browser/components/extensions/test/browser";

      function loadTab(url) {
        return browser.tabs.create({url});
      }

      loadTab(`http://example.co.jp/${BASE_PATH}/file_language_ja.html`).then(tab => {
        return browser.tabs.detectLanguage(tab.id).then(lang => {
          browser.test.assertEq("ja", lang, "Japanese document should be detected as Japanese");
          return browser.tabs.remove(tab.id);
        });
      }).then(() => {
        return loadTab(`http://example.co.jp/${BASE_PATH}/file_language_fr_en.html`);
      }).then(tab => {
        return browser.tabs.detectLanguage(tab.id).then(lang => {
          browser.test.assertEq("fr", lang, "French/English document should be detected as primarily French");
          return browser.tabs.remove(tab.id);
        });
      }).then(() => {
        browser.test.notifyPass("detectLanguage");
      }).catch(e => {
        browser.test.fail(`Error: ${e} :: ${e.stack}`);
        browser.test.notifyFail("detectLanguage");
      });
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("detectLanguage");

  yield extension.unload();
});
