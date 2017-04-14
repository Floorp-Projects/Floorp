"use strict";

/* global XPCOMUtils, NewTabSearchProvider, run_next_test, ok, equal, do_check_true, do_get_profile, Services */
/* jscs:disable requireCamelCaseOrUpperCaseIdentifiers */

const Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabSearchProvider",
    "resource:///modules/NewTabSearchProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ContentSearch",
                                  "resource:///modules/ContentSearch.jsm");

// ensure a profile exists
do_get_profile();

function run_test() {
  run_next_test();
}

function hasProp(obj) {
  return function(aProp) {
    ok(obj.hasOwnProperty(aProp), `expect to have property ${aProp}`);
  };
}

add_task(function* test_search() {
  ContentSearch.init();
  let observerPromise = new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      if (aData === "init-complete" && aTopic === "browser-search-service") {
        Services.obs.removeObserver(observer, "browser-search-service");
        resolve();
      }
    }, "browser-search-service");
  });
  Services.search.init();
  yield observerPromise;
  do_check_true(Services.search.isInitialized);

  // get initial state of search and check it has correct properties
  let state = yield NewTabSearchProvider.search.asyncGetState();
  let stateProps = hasProp(state);
  ["engines", "currentEngine"].forEach(stateProps);

  // check that the current engine is correct and has correct properties
  let {currentEngine} = state;
  equal(currentEngine.name, Services.search.currentEngine.name, "Current engine has been correctly set");
  var engineProps = hasProp(currentEngine);
  ["name", "placeholder", "iconBuffer"].forEach(engineProps);

  // create dummy test engines to test observer
  Services.search.addEngineWithDetails("TestSearch1", "", "", "", "GET",
    "http://example.com/?q={searchTerms}");
  Services.search.addEngineWithDetails("TestSearch2", "", "", "", "GET",
    "http://example.com/?q={searchTerms}");

  // set one of the dummy test engines to the default engine
  Services.search.defaultEngine = Services.search.getEngineByName("TestSearch1");

  // test that the event emitter is working by setting a new current engine "TestSearch2"
  let engineName = "TestSearch2";
  NewTabSearchProvider.search.init();

  // event emitter will fire when current engine is changed
  let promise = new Promise(resolve => {
    NewTabSearchProvider.search.once("browser-search-engine-modified", (name, data) => { // jshint ignore:line
      resolve([name, data.name]);
    });
  });

  // set a new current engine
  Services.search.currentEngine = Services.search.getEngineByName(engineName);
  let expectedEngineName = Services.search.currentEngine.name;

  // emitter should fire and return the new engine
  let [eventName, actualEngineName] = yield promise;
  equal(eventName, "browser-search-engine-modified", `emitter sent the correct event ${eventName}`);
  equal(expectedEngineName, actualEngineName, `emitter set the correct engine ${expectedEngineName}`);
  NewTabSearchProvider.search.uninit();
});
