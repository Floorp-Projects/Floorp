/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test needs to be split up. See bug 1258717.
requestLongerTimeout(4);
ignoreAllUncaughtExceptions();

XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHome.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");

const TEST_CONTENT_HELPER = "chrome://mochitests/content/browser/browser/base/" +
  "content/test/general/aboutHome_content_script.js";
var gRightsVersion = Services.prefs.getIntPref("browser.rights.version");

registerCleanupFunction(function() {
  // Ensure we don't pollute prefs for next tests.
  Services.prefs.clearUserPref("network.cookies.cookieBehavior");
  Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
  Services.prefs.clearUserPref("browser.rights.override");
  Services.prefs.clearUserPref("browser.rights." + gRightsVersion + ".shown");
});

add_task(function* () {
  info("Check that clearing cookies does not clear storage");

  yield withSnippetsMap(
    () => {
      Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService)
        .notifyObservers(null, "cookie-changed", "cleared");
    },
    function* () {
      isnot(content.gSnippetsMap.get("snippets-last-update"), null,
            "snippets-last-update should have a value");
    });
});

add_task(function* () {
  info("Check default snippets are shown");

  yield withSnippetsMap(null, function* () {
    let doc = content.document;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element")
    is(snippetsElt.getElementsByTagName("span").length, 1,
       "A default snippet is present.");
  });
});

add_task(function* () {
  info("Check default snippets are shown if snippets are invalid xml");

  yield withSnippetsMap(
    // This must set some incorrect xhtml code.
    snippetsMap => snippetsMap.set("snippets", "<p><b></p></b>"),
    function* () {
      let doc = content.document;
      let snippetsElt = doc.getElementById("snippets");
      ok(snippetsElt, "Found snippets element");
      is(snippetsElt.getElementsByTagName("span").length, 1,
         "A default snippet is present.");

      content.gSnippetsMap.delete("snippets");
    });
});

add_task(function* () {
  info("Check that performing a search fires a search event and records to Telemetry.");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    let currEngine = Services.search.currentEngine;
    let engine = yield promiseNewEngine("searchSuggestionEngine.xml");
    // Make this actually work in healthreport by giving it an ID:
    Object.defineProperty(engine.wrappedJSObject, "identifier",
                          { value: "org.mozilla.testsearchsuggestions" });

    let p = promiseContentSearchChange(browser, engine.name);
    Services.search.currentEngine = engine;
    yield p;

    yield ContentTask.spawn(browser, { expectedName: engine.name }, function* (args) {
      let engineName = content.wrappedJSObject.gContentSearchController.defaultEngine.name;
      is(engineName, args.expectedName, "Engine name in DOM should match engine we just added");
    });

    let numSearchesBefore = 0;
    // Get the current number of recorded searches.
    let histogramKey = engine.identifier + ".abouthome";
    try {
      let hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
      if (histogramKey in hs) {
        numSearchesBefore = hs[histogramKey].sum;
      }
    } catch (ex) {
      // No searches performed yet, not a problem, |numSearchesBefore| is 0.
    }

    let searchStr = "a search";

    let expectedURL = Services.search.currentEngine
      .getSubmission(searchStr, null, "homepage").uri.spec;
    let promise = waitForDocLoadAndStopIt(expectedURL, browser);

    // Perform a search to increase the SEARCH_COUNT histogram.
    yield ContentTask.spawn(browser, { searchStr }, function* (args) {
      let doc = content.document;
      info("Perform a search.");
      doc.getElementById("searchText").value = args.searchStr;
      doc.getElementById("searchSubmit").click();
    });

    yield promise;

    // Make sure the SEARCH_COUNTS histogram has the right key and count.
    let hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
    Assert.ok(histogramKey in hs, "histogram with key should be recorded");
    Assert.equal(hs[histogramKey].sum, numSearchesBefore + 1,
                 "histogram sum should be incremented");

    Services.search.currentEngine = currEngine;
    try {
      Services.search.removeEngine(engine);
    } catch (ex) {}
  });
});

add_task(function* () {
  info("Check snippets map is cleared if cached version is old");

  yield withSnippetsMap(
    snippetsMap => {
      snippetsMap.set("snippets", "test");
      snippetsMap.set("snippets-cached-version", 0);
    },
    function* () {
      let snippetsMap = content.gSnippetsMap;
      ok(!snippetsMap.has("snippets"), "snippets have been properly cleared");
      ok(!snippetsMap.has("snippets-cached-version"),
         "cached-version has been properly cleared");
    });
});

add_task(function* () {
  info("Check cached snippets are shown if cached version is current");

  yield withSnippetsMap(
    snippetsMap => snippetsMap.set("snippets", "test"),
    function* (args) {
      let doc = content.document;
      let snippetsMap = content.gSnippetsMap

      let snippetsElt = doc.getElementById("snippets");
      ok(snippetsElt, "Found snippets element");
      is(snippetsElt.innerHTML, "test", "Cached snippet is present.");

      is(snippetsMap.get("snippets"), "test", "snippets still cached");
      is(snippetsMap.get("snippets-cached-version"),
         args.expectedVersion,
         "cached-version is correct");
      ok(snippetsMap.has("snippets-last-update"), "last-update still exists");
    }, { expectedVersion: AboutHomeUtils.snippetsVersion });
});

add_task(function* () {
  info("Check if the 'Know Your Rights' default snippet is shown when " +
    "'browser.rights.override' pref is set and that its link works");

  Services.prefs.setBoolPref("browser.rights.override", false);

  ok(AboutHomeUtils.showKnowYourRights, "AboutHomeUtils.showKnowYourRights should be TRUE");

  yield withSnippetsMap(null, function* () {
    let doc = content.document;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    let linkEl = snippetsElt.querySelector("a");
    is(linkEl.href, "about:rights", "Snippet link is present.");
  }, null, function* () {
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, "about:rights");
    yield BrowserTestUtils.synthesizeMouseAtCenter("a[href='about:rights']", {
      button: 0
    }, gBrowser.selectedBrowser);
    yield loadPromise;
    is(gBrowser.currentURI.spec, "about:rights", "about:rights should have opened.");
  });


  Services.prefs.clearUserPref("browser.rights.override");
});

add_task(function* () {
  info("Check if the 'Know Your Rights' default snippet is NOT shown when " +
    "'browser.rights.override' pref is NOT set");

  Services.prefs.setBoolPref("browser.rights.override", true);

  let rightsData = AboutHomeUtils.knowYourRightsData;
  ok(!rightsData, "AboutHomeUtils.knowYourRightsData should be FALSE");

  yield withSnippetsMap(null, function*() {
    let doc = content.document;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    ok(snippetsElt.getElementsByTagName("a")[0].href != "about:rights",
      "Snippet link should not point to about:rights.");
  });

  Services.prefs.clearUserPref("browser.rights.override");
});

add_task(function* () {
  info("Check POST search engine support");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    return new Promise(resolve => {
      let searchObserver = Task.async(function* search_observer(subject, topic, data) {
        let currEngine = Services.search.defaultEngine;
        let engine = subject.QueryInterface(Ci.nsISearchEngine);
        info("Observer: " + data + " for " + engine.name);

        if (data != "engine-added")
          return;

        if (engine.name != "POST Search")
          return;

        Services.obs.removeObserver(searchObserver, "browser-search-engine-modified");

        // Ready to execute the tests!
        let needle = "Search for something awesome.";

        let p = promiseContentSearchChange(browser, engine.name);
        Services.search.defaultEngine = engine;
        yield p;

        let promise = BrowserTestUtils.browserLoaded(browser);

        yield ContentTask.spawn(browser, { needle }, function* (args) {
          let doc = content.document;
          doc.getElementById("searchText").value = args.needle;
          doc.getElementById("searchSubmit").click();
        });

        yield promise;

        // When the search results load, check them for correctness.
        yield ContentTask.spawn(browser, { needle }, function* (args) {
          let loadedText = content.document.body.textContent;
          ok(loadedText, "search page loaded");
          is(loadedText, "searchterms=" + escape(args.needle.replace(/\s/g, "+")),
             "Search text should arrive correctly");
        });

        Services.search.defaultEngine = currEngine;
        try {
          Services.search.removeEngine(engine);
        } catch (ex) {}
        resolve();
      });
      Services.obs.addObserver(searchObserver, "browser-search-engine-modified", false);
      Services.search.addEngine("http://test:80/browser/browser/base/content/test/general/POSTSearchEngine.xml",
                                null, null, false);
    });
  });
});

add_task(function* () {
  info("Make sure that a page can't imitate about:home");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    let promise = BrowserTestUtils.browserLoaded(browser);
    browser.loadURI("https://example.com/browser/browser/base/content/test/general/test_bug959531.html");
    yield promise;

    yield ContentTask.spawn(browser, null, function* () {
      let button = content.document.getElementById("settings");
      ok(button, "Found settings button in test page");
      button.click();
    });

    yield new Promise(resolve => {
      // It may take a few turns of the event loop before the window
      // is displayed, so we wait.
      function check(n) {
        let win = Services.wm.getMostRecentWindow("Browser:Preferences");
        ok(!win, "Preferences window not showing");
        if (win) {
          win.close();
        }

        if (n > 0) {
          executeSoon(() => check(n - 1));
        } else {
          resolve();
        }
      }

      check(5);
    });
  });
});

add_task(function* () {
  // See browser_contentSearchUI.js for comprehensive content search UI tests.
  info("Search suggestion smoke test");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    // Add a test engine that provides suggestions and switch to it.
    let currEngine = Services.search.currentEngine;
    let engine = yield promiseNewEngine("searchSuggestionEngine.xml");
    let p = promiseContentSearchChange(browser, engine.name);
    Services.search.currentEngine = engine;
    yield p;

    yield ContentTask.spawn(browser, null, function* () {
      // Type an X in the search input.
      let input = content.document.getElementById("searchText");
      input.focus();
    });

    yield BrowserTestUtils.synthesizeKey("x", {}, browser);

    yield ContentTask.spawn(browser, null, function* () {
      // Wait for the search suggestions to become visible.
      let table = content.document.getElementById("searchSuggestionTable");
      let input = content.document.getElementById("searchText");

      yield new Promise(resolve => {
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
    });

    // Empty the search input, causing the suggestions to be hidden.
    yield BrowserTestUtils.synthesizeKey("a", { accelKey: true }, browser);
    yield BrowserTestUtils.synthesizeKey("VK_DELETE", {}, browser);

    yield ContentTask.spawn(browser, null, function* () {
      let table = content.document.getElementById("searchSuggestionTable");
      yield ContentTaskUtils.waitForCondition(() => table.hidden,
        "Search suggestion table hidden");
    });

    Services.search.currentEngine = currEngine;
    try {
      Services.search.removeEngine(engine);
    } catch (ex) { }
  });
});

add_task(function* () {
  info("Clicking suggestion list while composing");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    // Add a test engine that provides suggestions and switch to it.
    let currEngine = Services.search.currentEngine;
    let engine = yield promiseNewEngine("searchSuggestionEngine.xml");
    let p = promiseContentSearchChange(browser, engine.name);
    Services.search.currentEngine = engine;
    yield p;

    yield ContentTask.spawn(browser, null, function* () {
      // Start composition and type "x"
      let input = content.document.getElementById("searchText");
      input.focus();
    });

    yield BrowserTestUtils.synthesizeComposition({
      type: "compositionstart",
      data: ""
    }, browser);
    yield BrowserTestUtils.synthesizeCompositionChange({
      composition: {
        string: "x",
        clauses: [
          { length: 1, attr: Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE }
        ]
      },
      caret: { start: 1, length: 0 }
    }, browser);

    yield ContentTask.spawn(browser, null, function* () {
      let searchController = content.wrappedJSObject.gContentSearchController;

      // Wait for the search suggestions to become visible.
      let table = searchController._suggestionsList;
      let input = content.document.getElementById("searchText");

      yield new Promise(resolve => {
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

    // Click the second suggestion.
    let expectedURL = Services.search.currentEngine
      .getSubmission("xbar", null, "homepage").uri.spec;
    let loadPromise = waitForDocLoadAndStopIt(expectedURL);
    yield BrowserTestUtils.synthesizeMouseAtCenter("#TEMPID", {
      button: 0
    }, browser);
    yield loadPromise;

    yield ContentTask.spawn(browser, null, function* () {
      let input = content.document.getElementById("searchText");
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

add_task(function* () {
  info("Pressing any key should focus the search box in the page, and send the key to it");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    yield BrowserTestUtils.synthesizeMouseAtCenter("#brandLogo", {}, browser);

    yield ContentTask.spawn(browser, null, function* () {
      let doc = content.document;
      isnot(doc.getElementById("searchText"), doc.activeElement,
        "Search input should not be the active element.");
    });

    yield BrowserTestUtils.synthesizeKey("a", {}, browser);

    yield ContentTask.spawn(browser, null, function* () {
      let doc = content.document;
      let searchInput = doc.getElementById("searchText");
      yield ContentTaskUtils.waitForCondition(() => doc.activeElement === searchInput,
        "Search input should be the active element.");
      is(searchInput.value, "a", "Search input should be 'a'.");
    });
  });
});

add_task(function* () {
  info("Cmd+k should focus the search box in the toolbar when it's present");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    yield BrowserTestUtils.synthesizeMouseAtCenter("#brandLogo", {}, browser);

    let doc = window.document;
    let searchInput = doc.getElementById("searchbar").textbox.inputField;
    isnot(searchInput, doc.activeElement, "Search bar should not be the active element.");

    EventUtils.synthesizeKey("k", { accelKey: true });
    yield promiseWaitForCondition(() => doc.activeElement === searchInput);
    is(searchInput, doc.activeElement, "Search bar should be the active element.");
  });
});

add_task(function* () {
  info("Sync button should open about:preferences#sync");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    let oldOpenPrefs = window.openPreferences;
    let openPrefsPromise = new Promise(resolve => {
      window.openPreferences = function(pane, params) {
        resolve({ pane, params });
      };
    });

    yield BrowserTestUtils.synthesizeMouseAtCenter("#sync", {}, browser);

    let result = yield openPrefsPromise;
    window.openPreferences = oldOpenPrefs;

    is(result.pane, "paneSync", "openPreferences should be called with paneSync");
    is(result.params.urlParams.entrypoint, "abouthome",
      "openPreferences should be called with abouthome entrypoint");
  });
});

add_task(function* () {
  info("Pressing Space while the Addons button is focused should activate it");

  // Skip this test on Mac, because Space doesn't activate the button there.
  if (AppConstants.platform == "macosx") {
    return;
  }

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, function* (browser) {
    info("Waiting for about:addons tab to open...");
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");

    yield ContentTask.spawn(browser, null, function* () {
      let addOnsButton = content.document.getElementById("addons");
      addOnsButton.focus();
    });
    yield BrowserTestUtils.synthesizeKey(" ", {}, browser);

    let tab = yield promiseTabOpened;
    is(tab.linkedBrowser.currentURI.spec, "about:addons",
      "Should have seen the about:addons tab");
    yield BrowserTestUtils.removeTab(tab);
  });
});

/**
 * Cleans up snippets and ensures that by default we don't try to check for
 * remote snippets since that may cause network bustage or slowness.
 *
 * @param aSetupFn
 *        The setup function to be run.
 * @param testFn
 *        the content task to run
 * @param testArgs (optional)
 *        the parameters to pass to the content task
 * @param parentFn (optional)
 *        the function to run in the parent after the content task has completed.
 * @return {Promise} resolved when the snippets are ready.  Gets the snippets map.
 */
function* withSnippetsMap(setupFn, testFn, testArgs = null, parentFn = null) {
  let setupFnSource;
  if (setupFn) {
    setupFnSource = setupFn.toSource();
  }

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
    let promiseAfterLocationChange = () => {
      return ContentTask.spawn(browser, {
        setupFnSource,
        version: AboutHomeUtils.snippetsVersion,
      }, function* (args) {
        return new Promise(resolve => {
          let document = content.document;
          // We're not using Promise-based listeners, because they resolve asynchronously.
          // The snippets test setup code relies on synchronous behaviour here.
          document.addEventListener("AboutHomeLoadSnippets", function loadSnippets() {
            document.removeEventListener("AboutHomeLoadSnippets", loadSnippets);

            let updateSnippets;
            if (args.setupFnSource) {
              updateSnippets = eval(`(() => (${args.setupFnSource}))()`);
            }

            content.wrappedJSObject.ensureSnippetsMapThen(snippetsMap => {
              snippetsMap = Cu.waiveXrays(snippetsMap);
              info("Got snippets map: " +
                   "{ last-update: " + snippetsMap.get("snippets-last-update") +
                   ", cached-version: " + snippetsMap.get("snippets-cached-version") +
                   " }");
              // Don't try to update.
              snippetsMap.set("snippets-last-update", Date.now());
              snippetsMap.set("snippets-cached-version", args.version);
              // Clear snippets.
              snippetsMap.delete("snippets");

              if (updateSnippets) {
                updateSnippets(snippetsMap);
              }

              // Tack it to the global object
              content.gSnippetsMap = snippetsMap;

              resolve();
            });
          });
        });
      });
    };

    // We'd like to listen to the 'AboutHomeLoadSnippets' event on a fresh
    // document as soon as technically possible, so we use webProgress.
    let promise = new Promise(resolve => {
      let wpl = {
        onLocationChange() {
          gBrowser.removeProgressListener(wpl);
          // Phase 2: retrieving the snippets map is the next promise on our agenda.
          promiseAfterLocationChange().then(resolve);
        },
        onProgressChange() {},
        onStatusChange() {},
        onSecurityChange() {}
      };
      gBrowser.addProgressListener(wpl);
    });

    // Set the URL to 'about:home' here to allow capturing the 'AboutHomeLoadSnippets'
    // event.
    browser.loadURI("about:home");
    // Wait for LocationChange.
    yield promise;

    yield ContentTask.spawn(browser, testArgs, testFn);
    if (parentFn) {
      yield parentFn();
    }
  });
}

function promiseContentSearchChange(browser, newEngineName) {
  return ContentTask.spawn(browser, { newEngineName }, function* (args) {
    return new Promise(resolve => {
      content.addEventListener("ContentSearchService", function listener(aEvent) {
        if (aEvent.detail.type == "CurrentState" &&
            content.wrappedJSObject.gContentSearchController.defaultEngine.name == args.newEngineName) {
          content.removeEventListener("ContentSearchService", listener);
          resolve();
        }
      });
    });
  });
}

function promiseNewEngine(basename) {
  info("Waiting for engine to be added: " + basename);
  return new Promise((resolve, reject) => {
    let url = getRootDirectory(gTestPath) + basename;
    Services.search.addEngine(url, null, "", false, {
      onSuccess(engine) {
        info("Search engine added: " + basename);
        registerCleanupFunction(() => {
          try {
            Services.search.removeEngine(engine);
          } catch (ex) { /* Can't remove the engine more than once */ }
        });
        resolve(engine);
      },
      onError(errCode) {
        ok(false, "addEngine failed with error code " + errCode);
        reject();
      },
    });
  });
}
