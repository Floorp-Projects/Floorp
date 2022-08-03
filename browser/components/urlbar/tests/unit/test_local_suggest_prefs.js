/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test for following preferences related to local suggest.
// * browser.urlbar.suggest.bookmark
// * browser.urlbar.suggest.history
// * browser.urlbar.suggest.openpage

testEngine_setup();

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.engines", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

  const uri = Services.io.newURI("http://example.com/");

  await PlacesTestUtils.addVisits([{ uri, title: "example" }]);
  await PlacesUtils.bookmarks.insert({
    url: uri,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  await addOpenPages(uri);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    Services.prefs.clearUserPref("browser.urlbar.suggest.engines");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");

    Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
    Services.prefs.clearUserPref("browser.urlbar.suggest.history");
    Services.prefs.clearUserPref("browser.urlbar.suggest.openpage");
    await cleanupPlaces();
  });
});

add_task(async function test_prefs() {
  const testData = [
    {
      bookmark: true,
      history: true,
      openpage: true,
    },
    {
      bookmark: false,
      history: true,
      openpage: true,
    },
    {
      bookmark: true,
      history: false,
      openpage: true,
    },
    {
      bookmark: true,
      history: true,
      openpage: false,
    },
    {
      bookmark: false,
      history: false,
      openpage: true,
    },
    {
      bookmark: false,
      history: true,
      openpage: false,
    },
    {
      bookmark: true,
      history: false,
      openpage: false,
    },
    {
      bookmark: false,
      history: false,
      openpage: false,
    },
  ];

  for (const { bookmark, history, openpage } of testData) {
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", bookmark);
    Services.prefs.setBoolPref("browser.urlbar.suggest.history", history);
    Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", openpage);

    info(`Test bookmark:${bookmark} history:${history} openpage:${openpage}`);

    const context = createContext("e", { isPrivate: false });
    const matches = [];

    matches.push(
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      })
    );

    if (openpage) {
      matches.push(
        makeTabSwitchResult(context, {
          uri: "http://example.com/",
          title: "example",
        })
      );
    } else if (bookmark) {
      matches.push(
        makeBookmarkResult(context, {
          uri: "http://example.com/",
          title: "example",
        })
      );
    } else if (history) {
      matches.push(
        makeVisitResult(context, {
          uri: "http://example.com/",
          title: "example",
        })
      );
    }

    await check_results({ context, matches });
  }
});
