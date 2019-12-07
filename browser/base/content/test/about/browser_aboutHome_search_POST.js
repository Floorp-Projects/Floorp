/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ignoreAllUncaughtExceptions();

add_task(async function() {
  info("Check POST search engine support");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function(
    browser
  ) {
    return new Promise(resolve => {
      let searchObserver = async function search_observer(
        subject,
        topic,
        data
      ) {
        let currEngine = await Services.search.getDefault();
        let engine = subject.QueryInterface(Ci.nsISearchEngine);
        info("Observer: " + data + " for " + engine.name);

        if (data != "engine-added") {
          return;
        }

        if (engine.name != "POST Search") {
          return;
        }

        Services.obs.removeObserver(
          searchObserver,
          "browser-search-engine-modified"
        );

        // Ready to execute the tests!
        let needle = "Search for something awesome.";

        await Promise.all([
          promiseContentSearchChange(browser, engine.name),
          Services.search.setDefault(engine),
        ]);
        let promise = BrowserTestUtils.browserLoaded(browser);
        await ContentTask.spawn(browser, { needle }, async function(args) {
          let doc = content.document;
          let el = doc.querySelector(["#searchText", "#newtab-search-text"]);
          el.value = args.needle;
          doc.getElementById("searchSubmit").click();
        });

        await promise;

        // When the search results load, check them for correctness.
        await ContentTask.spawn(browser, { needle }, async function(args) {
          let loadedText = content.document.body.textContent;
          ok(loadedText, "search page loaded");
          is(
            loadedText,
            "searchterms=" + escape(args.needle.replace(/\s/g, "+")),
            "Search text should arrive correctly"
          );
        });

        await Services.search.setDefault(currEngine);
        try {
          await Services.search.removeEngine(engine);
        } catch (ex) {}
        resolve();
      };
      Services.obs.addObserver(
        searchObserver,
        "browser-search-engine-modified"
      );
      Services.search.addEngine(
        "http://test:80/browser/browser/base/content/test/about/POSTSearchEngine.xml",
        null,
        false
      );
    });
  });
});
