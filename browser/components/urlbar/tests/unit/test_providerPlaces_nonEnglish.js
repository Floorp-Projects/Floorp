/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

Test autocomplete for non-English URLs

- add a visit for a page with a non-English URL
- search
- test number of matches (should be exactly one)

*/

testEngine_setup();

add_task(async function test_autocomplete_non_english() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  let searchTerm = "ユニコード";
  let unescaped = "http://www.foobar.com/" + searchTerm + "/";
  let uri = Services.io.newURI(unescaped);
  await PlacesTestUtils.addVisits(uri);
  let context = createContext(searchTerm, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri.spec,
        title: `test visit for ${uri.spec}`,
      }),
    ],
  });
});
