/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var gSS = Services.search;

function observer(aSubject, aTopic, aData) {
  if (!gCurrentTest) {
    info("Observer called with no test active");
    return;
  }

  let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
  info("Observer: " + aData + " for " + engine.name);
  let method;
  switch (aData) {
    case "engine-added":
      if (gCurrentTest.added)
        method = "added";
      break;
    case "engine-default":
      if (gCurrentTest.current)
        method = "current";
      break;
    case "engine-removed":
      if (gCurrentTest.removed)
        method = "removed";
      break;
  }

  if (method)
    gCurrentTest[method](engine);
}

function checkEngine(checkObj, engineObj) {
  info("Checking engine");
  for (let prop in checkObj)
    is(checkObj[prop], engineObj[prop], prop + " is correct");
}

var gTests = [
  {
    name: "opensearch install",
    engine: {
      name: "Foo",
      alias: null,
      description: "Foo Search",
      searchForm: "http://mochi.test:8888/browser/browser/components/search/test/browser/",
    },
    run() {
      Services.obs.addObserver(observer, "browser-search-engine-modified");

      gSS.addEngine("http://mochi.test:8888/browser/browser/components/search/test/browser/testEngine.xml",
                    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAABGklEQVQoz2NgGB6AnZ1dUlJSXl4eSDIyMhLW4Ovr%2B%2Fr168uXL69Zs4YoG%2BLi4i5dusTExMTGxsbNzd3f37937976%2BnpmZmagbHR09J49e5YvX66kpATVEBYW9ubNm2nTphkbG7e2tp44cQLIuHfvXm5urpaWFlDKysqqu7v73LlzECMYIiIiHj58mJCQoKKicvXq1bS0NKBgW1vbjh074uPjgeqAXE1NzSdPnvDz84M0AEUvXLgAsW379u1z5swBen3jxo2zZ892cHB4%2BvQp0KlAfwI1cHJyghQFBwfv2rULokFXV%2FfixYu7d%2B8GGqGgoMDKyrpu3br9%2B%2FcDuXl5eVA%2FAEWBfoWHAdAYoNuAYQ0XAeoUERFhGDYAAPoUaT2dfWJuAAAAAElFTkSuQmCC",
                    false);
    },
    added(engine) {
      ok(engine, "engine was added.");

      checkEngine(this.engine, engine);

      let engineFromSS = gSS.getEngineByName(this.engine.name);
      is(engine, engineFromSS, "engine is obtainable via getEngineByName");

      let aEngine = gSS.getEngineByAlias("fooalias");
      ok(!aEngine, "Alias was not parsed from engine description");

      gSS.defaultEngine = engine;
    },
    current(engine) {
      let defaultEngine = gSS.defaultEngine;
      is(engine, defaultEngine, "engine is current");
      is(engine.name, this.engine.name, "current engine was changed successfully");

      gSS.removeEngine(engine);
    },
    removed(engine) {
      // Remove the observer before calling the defaultEngine getter,
      // as that getter will set the defaultEngine to the original default
      // which will trigger a notification causing the test to loop over all
      // engines.
      Services.obs.removeObserver(observer, "browser-search-engine-modified");

      let defaultEngine = gSS.defaultEngine;
      ok(defaultEngine, "An engine is present.");
      isnot(defaultEngine.name, this.engine.name, "Current engine reset after removal");

      nextTest();
    },
  },
];

var gCurrentTest = null;
function nextTest() {
  if (gTests.length) {
    gCurrentTest = gTests.shift();
    info("Running " + gCurrentTest.name);
    gCurrentTest.run();
  } else {
    executeSoon(finish);
  }
}

function test() {
  waitForExplicitFinish();
  gSS.init().then(nextTest);
}
