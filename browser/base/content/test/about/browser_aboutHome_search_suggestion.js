/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ignoreAllUncaughtExceptions();

add_task(async function () {
  // See browser_contentSearchUI.js for comprehensive content search UI tests.
  info("Search suggestion smoke test");

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
        false,
      ],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:home" },
    async function (browser) {
      // Add a test engine that provides suggestions and switch to it.
      let engine;
      await promiseContentSearchChange(browser, async () => {
        engine = await SearchTestUtils.promiseNewSearchEngine({
          url: getRootDirectory(gTestPath) + "searchSuggestionEngine.xml",
          setAsDefault: true,
        });
        return engine.name;
      });

      await SpecialPowers.spawn(browser, [], async function () {
        // Type an X in the search input.
        let input = content.document.querySelector([
          "#searchText",
          "#newtab-search-text",
        ]);
        input.focus();
      });

      await BrowserTestUtils.synthesizeKey("x", {}, browser);

      await SpecialPowers.spawn(browser, [], async function () {
        // Wait for the search suggestions to become visible.
        let table = content.document.getElementById("searchSuggestionTable");
        let input = content.document.querySelector([
          "#searchText",
          "#newtab-search-text",
        ]);

        await ContentTaskUtils.waitForMutationCondition(
          input,
          { attributeFilter: ["aria-expanded"] },
          () => input.getAttribute("aria-expanded") == "true"
        );
        ok(!table.hidden, "Search suggestion table unhidden");
      });

      // Empty the search input, causing the suggestions to be hidden.
      await BrowserTestUtils.synthesizeKey("a", { accelKey: true }, browser);
      await BrowserTestUtils.synthesizeKey("VK_DELETE", {}, browser);

      await SpecialPowers.spawn(browser, [], async function () {
        let table = content.document.getElementById("searchSuggestionTable");
        await ContentTaskUtils.waitForCondition(
          () => table.hidden,
          "Search suggestion table hidden"
        );
      });
    }
  );
  await SpecialPowers.popPrefEnv();
});
