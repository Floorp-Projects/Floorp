/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 422698 to make sure searches with urls from the location bar
 * correctly match itself when it contains escaped characters.
 */

testEngine_setup();

add_task(async function test_escape() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  let uri1 = Services.io.newURI("http://unescapeduri/");
  let uri2 = Services.io.newURI("http://escapeduri/%40/");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
  ]);

  info("Unescaped location matches itself");
  let context = createContext("http://unescapeduri/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "title",
        iconUri: `page-icon:${uri1.spec}`,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        heuristic: true,
      }),
      // Note that uri2 does not appear in results.
    ],
  });

  info("Escaped location matches itself");
  context = createContext("http://escapeduri/%40", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://escapeduri/%40",
        fallbackTitle: "http://escapeduri/@",
        iconUri: "page-icon:http://escapeduri/",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
