/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await gCUITestUtils.addSearchBar();
  await clearSearchbarHistory();
  let defaultEngine = await Services.search.getDefault();

  await SearchTestUtils.installSearchExtension({
    id: "test",
    name: "test",
    suggest_url:
      "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
    suggest_url_get_params: "query={searchTerms}",
  });

  await Services.search.setDefault(
    await Services.search.getEngineByName("test")
  );

  registerCleanupFunction(async () => {
    await clearSearchbarHistory();
    gCUITestUtils.removeSearchBar();
    await Services.search.setDefault(defaultEngine);
  });
});

async function check_results(input, expected) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async browser => {
      let popup = await searchInSearchbar(input);

      const listItemElems = popup.richlistbox.querySelectorAll(
        ".autocomplete-richlistitem"
      );

      Assert.deepEqual(
        Array.from(listItemElems)
          .filter(e => !e.collapsed)
          .map(e => e.getAttribute("title")),
        expected,
        "Should have received the expected suggestions"
      );

      // Now visit the search to put an item in form history.
      let p = BrowserTestUtils.browserLoaded(browser);
      EventUtils.synthesizeKey("KEY_Enter");
      await p;
    }
  );
}

add_task(async function test_utf8_results() {
  await check_results("。", ["。foo", "。bar"]);

  // The first run added the entry into form history, check that is correct
  // as well.
  await check_results("。", ["。", "。foo", "。bar"]);
});
