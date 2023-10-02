/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var gSS = Services.search;

function test() {
  waitForExplicitFinish();

  function observer(aSubject, aTopic, aData) {
    switch (aData) {
      case "engine-added":
        let engine = gSS.getEngineByName("483086a");
        ok(engine, "Test engine 1 installed");
        isnot(
          engine.searchForm,
          "foo://example.com",
          "Invalid SearchForm URL dropped"
        );
        gSS.removeEngine(engine);
        break;
      case "engine-removed":
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        test2();
        break;
    }
  }

  Services.obs.addObserver(observer, "browser-search-engine-modified");
  gSS.addOpenSearchEngine(
    "http://mochi.test:8888/browser/browser/components/search/test/browser/483086-1.xml",
    "data:image/x-icon;%00"
  );
}

function test2() {
  function observer(aSubject, aTopic, aData) {
    switch (aData) {
      case "engine-added":
        let engine = gSS.getEngineByName("483086b");
        ok(engine, "Test engine 2 installed");
        is(engine.searchForm, "https://example.com", "SearchForm is correct");
        gSS.removeEngine(engine);
        break;
      case "engine-removed":
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        finish();
        break;
    }
  }

  Services.obs.addObserver(observer, "browser-search-engine-modified");
  gSS.addOpenSearchEngine(
    "http://mochi.test:8888/browser/browser/components/search/test/browser/483086-2.xml",
    "data:image/x-icon;%00"
  );
}
