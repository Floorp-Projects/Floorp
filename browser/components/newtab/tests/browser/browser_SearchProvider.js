/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals ok, is, Services */
"use strict";
let imports = {};
Components.utils.import("resource:///modules/SearchProvider.jsm", imports);

// create test engine called MozSearch
Services.search.addEngineWithDetails("TestSearch", "", "", "", "GET",
  "http://example.com/?q={searchTerms}");
Services.search.defaultEngine = Services.search.getEngineByName("TestSearch");

function hasProp(obj) {
  return function(aProp) {
    ok(obj.hasOwnProperty(aProp), `expect to have property ${aProp}`);
  };
}

add_task(function* testState() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:newTab",
  }, function* () {

    ok(imports.SearchProvider, "Search provider was created");

    // state returns promise and eventually returns a state object
    var state = yield imports.SearchProvider.state;
    var stateProps = hasProp(state);
    ["engines", "currentEngine"].forEach(stateProps);

    var {
      engines
    } = state;

    // engines should be an iterable
    var proto = Object.getPrototypeOf(engines);
    var isIterable = Object.getOwnPropertySymbols(proto)[0] === Symbol.iterator;
    ok(isIterable, "Engines should be iterable.");

    // current engine should be the current engine from Services.search
    var {
      currentEngine
    } = state;
    is(currentEngine.name, Services.search.currentEngine.name, "Current engine has been correctly set to default engine");

    // current engine should properties
    var engineProps = hasProp(currentEngine);
    ["name", "placeholder", "iconBuffer", "logoBuffer", "logo2xBuffer", "preconnectOrigin"].forEach(engineProps);

  });
});

add_task(function* testSearch() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:newTab",
  }, function* () {

    // perform a search
    var searchData = {
      engineName: Services.search.currentEngine.name,
      searchString: "test",
      healthReportKey: "newtab",
      searchPurpose: "newtab",
      originalEvent: {
        shiftKey: false,
        ctrlKey: false,
        metaKey: false,
        altKey: false,
        button: false,
      },
    };

    // adding an entry to the form history will trigger a 'formhistory-add' notification, so we need to wait for
    // this to resolve before checking that the search string has been added to the suggestions list
    let addHistoryPromise = new Promise((resolve, reject) => {
      Services.obs.addObserver(function onAdd(subject, topic, data) { // jshint ignore:line
        if (data === "formhistory-add") {
          Services.obs.removeObserver(onAdd, "satchel-storage-changed");
          resolve(data);
        } else {
          reject();
        }
      }, "satchel-storage-changed", false);
    });

    var win = yield imports.SearchProvider.performSearch(gBrowser, searchData);
    var result = yield new Promise(resolve => {
      const pageShow = function() {
        win.gBrowser.tabs[1].linkedBrowser.removeEventListener("pageshow", pageShow);
        var tab = win.gBrowser.tabContainer.tabbrowser;
        var url = tab.selectedTab.linkedBrowser.contentWindow.location.href;
        BrowserTestUtils.removeTab(tab.selectedTab);
        resolve(url);
      };

      ok(win.gBrowser.tabs[1], "search opened a new tab");
      win.gBrowser.tabs[1].linkedBrowser.addEventListener("pageshow", pageShow);
    });
    is(result, "http://example.com/?q=test", "should match search URL of default engine.");

    // suggestions has correct properties
    var suggestionData = {
      engineName: Services.search.currentEngine.name,
      searchString: "test",
    };
    var suggestions = yield imports.SearchProvider.getSuggestions(gBrowser, suggestionData);
    var suggestionProps = hasProp(suggestions);
    ["engineName", "searchString", "formHistory", "remote"].forEach(suggestionProps);

    // ensure that the search string has been added to the form history suggestions
    yield addHistoryPromise;
    suggestions = yield imports.SearchProvider.getSuggestions(gBrowser, suggestionData);
    var {
      formHistory
    } = suggestions;
    ok(formHistory.length !== 0, "a form history was created");
    is(formHistory[0], searchData.searchString, "the search string has been added to form history");

    // remove the entry we just added from the form history and ensure it no longer appears as a suggestion
    let removeHistoryPromise = new Promise((resolve, reject) => {
      Services.obs.addObserver(function onAdd(subject, topic, data) { // jshint ignore:line
        if (data === "formhistory-remove") {
          Services.obs.removeObserver(onAdd, "satchel-storage-changed");
          resolve(data);
        } else {
          reject();
        }
      }, "satchel-storage-changed", false);
    });

    yield imports.SearchProvider.removeFormHistoryEntry(gBrowser, searchData.searchString);
    yield removeHistoryPromise;
    suggestions = yield imports.SearchProvider.getSuggestions(gBrowser, suggestionData);
    ok(suggestions.formHistory.length === 0, "entry has been removed from form history");
  });
});

add_task(function* testFetchFailure() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:newTab",
  }, function* () {

    // ensure that the fetch failure is handled when the suggestions return a null value due to their
    // asynchronous nature
    let { controller } = imports.SearchProvider._suggestionDataForBrowser(gBrowser);
    let oldFetch = controller.fetch;
    controller.fetch = function(searchTerm, privateMode, engine) { //jshint ignore:line
      let promise = new Promise((resolve, reject) => { //jshint ignore:line
        reject();
      });
      return promise;
    };

    // this should throw, since the promise rejected
    let suggestionData = {
      engineName: Services.search.currentEngine.name,
      searchString: "test",
    };

    let suggestions = yield imports.SearchProvider.getSuggestions(gBrowser, suggestionData);

    ok(suggestions === null, "suggestions returned null and the function handled the rejection");
    controller.fetch = oldFetch;
  });
});
