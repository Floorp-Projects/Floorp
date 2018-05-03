/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ignoreAllUncaughtExceptions();

add_task(async function() {
  info("Clicking suggestion list while composing");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    // Add a test engine that provides suggestions and switch to it.
    let currEngine = Services.search.currentEngine;
    let engine = await promiseNewEngine("searchSuggestionEngine.xml");
    let p = promiseContentSearchChange(browser, engine.name);
    Services.search.currentEngine = engine;
    await p;

    await ContentTask.spawn(browser, null, async function() {
      // Start composition and type "x"
      let input = content.document.querySelector(["#searchText", "#newtab-search-text"]);
      input.focus();
    });

    info("Setting up the mutation observer before synthesizing composition");
    let mutationPromise = ContentTask.spawn(browser, null, async function() {
      let searchController = content.wrappedJSObject.gContentSearchController;

      // Wait for the search suggestions to become visible.
      let table = searchController._suggestionsList;
      let input = content.document.querySelector(["#searchText", "#newtab-search-text"]);

      await new Promise(resolve => {
        let observer = new content.MutationObserver(() => {
          if (input.getAttribute("aria-expanded") == "true") {
            observer.disconnect();
            ok(!table.hidden, "Search suggestion table unhidden");
            resolve();
          }
        });
        observer.observe(input, {
          attributes: true,
          attributeFilter: ["aria-expanded"],
        });
      });

      let row = table.children[1];
      row.setAttribute("id", "TEMPID");

      // ContentSearchUIController looks at the current selectedIndex when
      // performing a search. Synthesizing the mouse event on the suggestion
      // doesn't actually mouseover the suggestion and trigger it to be flagged
      // as selected, so we manually select it first.
      searchController.selectedIndex = 1;
    });

    // FYI: "compositionstart" will be dispatched automatically.
    await BrowserTestUtils.synthesizeCompositionChange({
      composition: {
        string: "x",
        clauses: [
          { length: 1, attr: Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE }
        ]
      },
      caret: { start: 1, length: 0 }
    }, browser);

    info("Waiting for search suggestion table unhidden");
    await mutationPromise;

    // Click the second suggestion.
    let expectedURL = Services.search.currentEngine
      .getSubmission("xbar", null, "homepage").uri.spec;
    let loadPromise = waitForDocLoadAndStopIt(expectedURL);
    await BrowserTestUtils.synthesizeMouseAtCenter("#TEMPID", {
      button: 0
    }, browser);
    await loadPromise;

    await ContentTask.spawn(browser, null, async function() {
      let input = content.document.querySelector(["#searchText", "#newtab-search-text"]);
      ok(input.value == "x", "Input value did not change");

      let row = content.document.getElementById("TEMPID");
      if (row) {
        row.removeAttribute("id");
      }
    });

    Services.search.currentEngine = currEngine;
    try {
      Services.search.removeEngine(engine);
    } catch (ex) { }
  });
});
