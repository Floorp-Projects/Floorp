/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const cachedPref = Services.prefs.getCharPref("browser.search.hiddenOneOffs");
const testPref = "Foo,FooDupe";

function promiseNewEngine(basename) {
  return new Promise((resolve, reject) => {
    info("Waiting for engine to be added: " + basename);
    Services.search.init({
      onInitComplete: function() {
        let url = getRootDirectory(gTestPath) + basename;
        Services.search.addEngine(url, Ci.nsISearchEngine.TYPE_MOZSEARCH, "", false, {
          onSuccess: function (engine) {
            info("Search engine added: " + basename);
            resolve(engine);
          },
          onError: function (errCode) {
            ok(false, "addEngine failed with error code " + errCode);
            reject();
          }
        });
      }
    });
  });
}

add_task(function* test_remove() {
  yield promiseNewEngine("testEngine_dupe.xml");
  yield promiseNewEngine("testEngine.xml");
  Services.prefs.setCharPref("browser.search.hiddenOneOffs", testPref);

  info("Removing testEngine_dupe.xml");
  Services.search.removeEngine(Services.search.getEngineByName("FooDupe"));

  let hiddenOneOffs =
    Services.prefs.getCharPref("browser.search.hiddenOneOffs").split(",");

  is(hiddenOneOffs.length, 1,
     "hiddenOneOffs has the correct engine count post removal.");
  is(hiddenOneOffs.some(x => x == "FooDupe"), false,
     "Removed Engine is not in hiddenOneOffs after removal");
  is(hiddenOneOffs.some(x => x == "Foo"), true,
     "Current hidden engine is not affected by removal.");

  info("Removing testEngine.xml");
  Services.search.removeEngine(Services.search.getEngineByName("Foo"));

  is(Services.prefs.getCharPref("browser.search.hiddenOneOffs"), "",
     "hiddenOneOffs is empty after removing all hidden engines.");
});

add_task(function* test_add() {
  yield promiseNewEngine("testEngine.xml");
  info("setting prefs to " + testPref);
  Services.prefs.setCharPref("browser.search.hiddenOneOffs", testPref);
  yield promiseNewEngine("testEngine_dupe.xml");

  let hiddenOneOffs =
    Services.prefs.getCharPref("browser.search.hiddenOneOffs").split(",");

  is(hiddenOneOffs.length, 1,
     "hiddenOneOffs has the correct number of hidden engines present post add.");
  is(hiddenOneOffs.some(x => x == "FooDupe"), false,
     "Added engine is not present in hidden list.");
  is(hiddenOneOffs.some(x => x == "Foo"), true,
     "Adding an engine does not remove engines from hidden list.");
});

registerCleanupFunction(() => {
  info("Removing testEngine.xml");
  Services.search.removeEngine(Services.search.getEngineByName("Foo"));
  info("Removing testEngine_dupe.xml");
  Services.search.removeEngine(Services.search.getEngineByName("FooDupe"));
  Services.prefs.setCharPref("browser.search.hiddenOneOffs", cachedPref);
});
