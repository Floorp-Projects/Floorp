/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testDetectLanguage() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: async function() {
      const BASE_PATH = "browser/browser/components/extensions/test/browser";

      function loadTab(url) {
        return browser.tabs.create({url});
      }

      try {
        let tab = await loadTab(`http://example.co.jp/${BASE_PATH}/file_language_ja.html`);
        let lang = await browser.tabs.detectLanguage(tab.id);
        browser.test.assertEq("ja", lang, "Japanese document should be detected as Japanese");
        await browser.tabs.remove(tab.id);

        tab = await loadTab(`http://example.co.jp/${BASE_PATH}/file_language_fr_en.html`);
        lang = await browser.tabs.detectLanguage(tab.id);
        browser.test.assertEq("fr", lang, "French/English document should be detected as primarily French");
        await browser.tabs.remove(tab.id);

        tab = await loadTab(`http://example.co.jp/${BASE_PATH}/file_language_tlh.html`);
        lang = await browser.tabs.detectLanguage(tab.id);
        browser.test.assertEq("und", lang, "Klingon document should not be detected, should return 'und'");
        await browser.tabs.remove(tab.id);

        browser.test.notifyPass("detectLanguage");
      } catch (e) {
        browser.test.fail(`Error: ${e} :: ${e.stack}`);
        browser.test.notifyFail("detectLanguage");
      }
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("detectLanguage");

  yield extension.unload();
});
