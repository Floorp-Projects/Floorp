/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const testPref = "Foo,FooDupe";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.search.hiddenOneOffs");
});

add_task(async function test_remove() {
  await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine_dupe.xml"
  );
  await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine.xml"
  );
  Services.prefs.setCharPref("browser.search.hiddenOneOffs", testPref);

  info("Removing testEngine_dupe.xml");
  await Services.search.removeEngine(
    Services.search.getEngineByName("FooDupe")
  );

  let hiddenOneOffs = Services.prefs
    .getCharPref("browser.search.hiddenOneOffs")
    .split(",");

  is(
    hiddenOneOffs.length,
    1,
    "hiddenOneOffs has the correct engine count post removal."
  );
  is(
    hiddenOneOffs.some(x => x == "FooDupe"),
    false,
    "Removed Engine is not in hiddenOneOffs after removal"
  );
  is(
    hiddenOneOffs.some(x => x == "Foo"),
    true,
    "Current hidden engine is not affected by removal."
  );

  info("Removing testEngine.xml");
  await Services.search.removeEngine(Services.search.getEngineByName("Foo"));

  is(
    Services.prefs.getCharPref("browser.search.hiddenOneOffs"),
    "",
    "hiddenOneOffs is empty after removing all hidden engines."
  );
});

add_task(async function test_add() {
  await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine.xml"
  );
  info("setting prefs to " + testPref);
  Services.prefs.setCharPref("browser.search.hiddenOneOffs", testPref);
  await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine_dupe.xml"
  );
  let hiddenOneOffs = Services.prefs
    .getCharPref("browser.search.hiddenOneOffs")
    .split(",");

  is(
    hiddenOneOffs.length,
    1,
    "hiddenOneOffs has the correct number of hidden engines present post add."
  );
  is(
    hiddenOneOffs.some(x => x == "FooDupe"),
    false,
    "Added engine is not present in hidden list."
  );
  is(
    hiddenOneOffs.some(x => x == "Foo"),
    true,
    "Adding an engine does not remove engines from hidden list."
  );
});

add_task(async function test_diacritics() {
  const diacritic_engine = "Foo \u2661";
  let Preferences = ChromeUtils.import(
    "resource://gre/modules/Preferences.jsm",
    {}
  ).Preferences;

  Preferences.set("browser.search.hiddenOneOffs", diacritic_engine);
  await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine_diacritics.xml"
  );

  let hiddenOneOffs = Preferences.get("browser.search.hiddenOneOffs").split(
    ","
  );
  is(
    hiddenOneOffs.some(x => x == diacritic_engine),
    false,
    "Observer cleans up added hidden engines that include a diacritic."
  );

  Preferences.set("browser.search.hiddenOneOffs", diacritic_engine);

  info("Removing testEngine_diacritics.xml");
  await Services.search.removeEngine(
    Services.search.getEngineByName(diacritic_engine)
  );

  hiddenOneOffs = Preferences.get("browser.search.hiddenOneOffs").split(",");
  is(
    hiddenOneOffs.some(x => x == diacritic_engine),
    false,
    "Observer cleans up removed hidden engines that include a diacritic."
  );
});
