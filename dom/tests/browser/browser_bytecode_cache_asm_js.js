"use strict";

const PAGE_URL =
  "http://example.com/browser/dom/tests/browser/page_bytecode_cache_asm_js.html";

add_task(async function () {
  // Eagerly generate bytecode cache.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.script_loader.bytecode_cache.enabled", true],
      ["dom.script_loader.bytecode_cache.strategy", -1],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE_URL,
      waitForLoad: true,
    },
    async browser => {
      let result = await SpecialPowers.spawn(browser, [], () => {
        return content.document.getElementById("result").textContent;
      });
      // No error shoud be caught by content.
      is(result, "ok");
    }
  );

  await SpecialPowers.popPrefEnv();
});
