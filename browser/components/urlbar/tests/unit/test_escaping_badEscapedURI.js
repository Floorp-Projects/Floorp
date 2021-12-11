/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 422277 to make sure bad escaped uris don't get escaped. This makes
 * sure we don't hit an assertion for "not a UTF8 string".
 */

testEngine_setup();

add_task(async function test() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  info("Bad escaped uri stays escaped");
  let uri1 = Services.io.newURI("http://site/%EAid");
  await PlacesTestUtils.addVisits([{ uri: uri1, title: "title" }]);
  let context = createContext("site", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "title",
      }),
    ],
  });
  await cleanupPlaces();
});
