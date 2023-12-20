/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the muxer functionality that hides URLs in history that were
// originally sponsored.

"use strict";

add_task(async function test() {
  // Disable search suggestions to avoid hitting the network.
  UrlbarPrefs.set("suggest.searches", false);

  let engine = await Services.search.getDefault();
  let pref = "browser.newtabpage.activity-stream.hideTopSitesWithSearchParam";

  // This maps URL search params to objects describing whether a URL with those
  // params is expected to appear in the search results. Each inner object maps
  // from a value of the pref to whether the URL is expected to appear given the
  // pref value.
  let tests = {
    "": {
      "": true,
      test: true,
      "test=": true,
      "test=hide": true,
      nomatch: true,
      "nomatch=": true,
      "nomatch=hide": true,
    },
    test: {
      "": true,
      test: false,
      "test=": false,
      "test=hide": true,
      nomatch: true,
      "nomatch=": true,
      "nomatch=hide": true,
    },
    "test=hide": {
      "": true,
      test: false,
      "test=": true,
      "test=hide": false,
      nomatch: true,
      "nomatch=": true,
      "nomatch=hide": true,
    },
    "test=foo&test=hide": {
      "": true,
      test: false,
      "test=": true,
      "test=hide": false,
      nomatch: true,
      "nomatch=": true,
      "nomatch=hide": true,
    },
  };

  for (let [urlParams, expected] of Object.entries(tests)) {
    for (let [prefValue, shouldAppear] of Object.entries(expected)) {
      info(
        "Running test: " +
          JSON.stringify({ urlParams, prefValue, shouldAppear })
      );

      // Add a visit to a URL with search params `urlParams`.
      let url = new URL("http://example.com/");
      url.search = urlParams;
      await PlacesTestUtils.addVisits(url);

      // Set the pref to `prefValue`.
      Services.prefs.setCharPref(pref, prefValue);

      // Set up the context and expected results. If `shouldAppear` is true, a
      // visit result for the URL should appear.
      let context = createContext("ample", { isPrivate: false });
      let expectedResults = [
        makeSearchResult(context, {
          heuristic: true,
          engineName: engine.name,
          engineIconUri: engine.getIconURL(),
        }),
      ];
      if (shouldAppear) {
        expectedResults.push(
          makeVisitResult(context, {
            uri: url.toString(),
            title: "test visit for " + url,
          })
        );
      }

      // Do a search and check the results.
      await check_results({
        context,
        matches: expectedResults,
      });

      await PlacesUtils.history.clear();
    }
  }

  Services.prefs.clearUserPref(pref);
});
