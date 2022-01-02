/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 424509 to make sure searching for "h" doesn't match "http" of urls.
 */

testEngine_setup();

add_task(async function test_escape() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  let uri1 = Services.io.newURI("http://site/");
  let uri2 = Services.io.newURI("http://happytimes/");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
  ]);

  info("Searching for h matches site and not http://");
  let context = createContext("h", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri2.spec,
        title: "title",
      }),
    ],
  });

  await cleanupPlaces();
});
