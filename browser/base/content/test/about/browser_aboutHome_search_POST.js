/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ignoreAllUncaughtExceptions();

add_task(async function () {
  info("Check POST search engine support");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
        false,
      ],
    ],
  });

  let currEngine = await Services.search.getDefault();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:home" },
    async browser => {
      let engine;
      await promiseContentSearchChange(browser, async () => {
        engine = await SearchTestUtils.promiseNewSearchEngine({
          url: "https://example.com/browser/browser/base/content/test/about/POSTSearchEngine.xml",
          setAsDefault: true,
        });
        return engine.name;
      });

      // Ready to execute the tests!
      let needle = "Search for something awesome.";

      let promise = BrowserTestUtils.browserLoaded(browser);
      await SpecialPowers.spawn(browser, [{ needle }], async function (args) {
        let doc = content.document;
        let el = doc.querySelector(["#searchText", "#newtab-search-text"]);
        el.value = args.needle;
        doc.getElementById("searchSubmit").click();
      });

      await promise;

      // When the search results load, check them for correctness.
      await SpecialPowers.spawn(browser, [{ needle }], async function (args) {
        let loadedText = content.document.body.textContent;
        ok(loadedText, "search page loaded");
        is(
          loadedText,
          "searchterms=" + escape(args.needle.replace(/\s/g, "+")),
          "Search text should arrive correctly"
        );
      });

      await Services.search.setDefault(
        currEngine,
        Ci.nsISearchService.CHANGE_REASON_UNKNOWN
      );
      try {
        await Services.search.removeEngine(engine);
      } catch (ex) {}
    }
  );
  await SpecialPowers.popPrefEnv();
});
