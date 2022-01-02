/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

Autocomplete Frecency Tests

- add a visit for each score permutation
- search
- test number of matches
- test each item's location in results

*/

testEngine_setup();

try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
    Ci.nsINavHistoryService
  );
} catch (ex) {
  do_throw("Could not get services\n");
}

var bucketPrefs = [
  ["firstBucketCutoff", "firstBucketWeight"],
  ["secondBucketCutoff", "secondBucketWeight"],
  ["thirdBucketCutoff", "thirdBucketWeight"],
  ["fourthBucketCutoff", "fourthBucketWeight"],
  [null, "defaultBucketWeight"],
];

var bonusPrefs = {
  embedVisitBonus: PlacesUtils.history.TRANSITION_EMBED,
  framedLinkVisitBonus: PlacesUtils.history.TRANSITION_FRAMED_LINK,
  linkVisitBonus: PlacesUtils.history.TRANSITION_LINK,
  typedVisitBonus: PlacesUtils.history.TRANSITION_TYPED,
  bookmarkVisitBonus: PlacesUtils.history.TRANSITION_BOOKMARK,
  downloadVisitBonus: PlacesUtils.history.TRANSITION_DOWNLOAD,
  permRedirectVisitBonus: PlacesUtils.history.TRANSITION_REDIRECT_PERMANENT,
  tempRedirectVisitBonus: PlacesUtils.history.TRANSITION_REDIRECT_TEMPORARY,
  reloadVisitBonus: PlacesUtils.history.TRANSITION_RELOAD,
};

// create test data
var searchTerm = "frecency";
var results = [];
var now = Date.now();
var prefPrefix = "places.frecency.";

async function task_initializeBucket(bucket) {
  let [cutoffName, weightName] = bucket;
  // get pref values
  let weight = Services.prefs.getIntPref(prefPrefix + weightName, 0);
  let cutoff = Services.prefs.getIntPref(prefPrefix + cutoffName, 0);
  if (cutoff < 1) {
    return;
  }

  // generate a date within the cutoff period
  let dateInPeriod = (now - (cutoff - 1) * 86400 * 1000) * 1000;

  for (let [bonusName, visitType] of Object.entries(bonusPrefs)) {
    let frecency = -1;
    let calculatedURI = null;
    let matchTitle = "";
    let bonusValue = Services.prefs.getIntPref(prefPrefix + bonusName);
    // unvisited (only for first cutoff date bucket)
    if (
      bonusName == "unvisitedBookmarkBonus" ||
      bonusName == "unvisitedTypedBonus"
    ) {
      if (cutoffName == "firstBucketCutoff") {
        let points = Math.ceil((bonusValue / parseFloat(100.0)) * weight);
        let visitCount = 1; // bonusName == "unvisitedBookmarkBonus" ? 1 : 0;
        frecency = Math.ceil(visitCount * points);
        calculatedURI = Services.io.newURI(
          "http://" +
            searchTerm +
            ".com/" +
            bonusName +
            ":" +
            bonusValue +
            "/cutoff:" +
            cutoff +
            "/weight:" +
            weight +
            "/frecency:" +
            frecency
        );
        if (bonusName == "unvisitedBookmarkBonus") {
          matchTitle = searchTerm + "UnvisitedBookmark";
          await PlacesUtils.bookmarks.insert({
            parentGuid: PlacesUtils.bookmarks.unfiledGuid,
            url: calculatedURI,
            title: matchTitle,
          });
        } else {
          matchTitle = searchTerm + "UnvisitedTyped";
          await PlacesTestUtils.addVisits({
            uri: calculatedURI,
            title: matchTitle,
            transition: visitType,
            visitDate: now,
          });
          histsvc.markPageAsTyped(calculatedURI);
        }
      }
    } else {
      // visited
      // visited bookmarks get the visited bookmark bonus twice
      if (visitType == Ci.nsINavHistoryService.TRANSITION_BOOKMARK) {
        bonusValue = bonusValue * 2;
      }

      let points = Math.ceil(
        (1 * ((bonusValue / parseFloat(100.0)).toFixed(6) * weight)) / 1
      );
      if (!points) {
        if (
          visitType == Ci.nsINavHistoryService.TRANSITION_EMBED ||
          visitType == Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK ||
          visitType == Ci.nsINavHistoryService.TRANSITION_DOWNLOAD ||
          visitType == Ci.nsINavHistoryService.TRANSITION_RELOAD ||
          bonusName == "defaultVisitBonus"
        ) {
          frecency = 0;
        } else {
          frecency = -1;
        }
      } else {
        frecency = points;
      }
      calculatedURI = Services.io.newURI(
        "http://" +
          searchTerm +
          ".com/" +
          bonusName +
          ":" +
          bonusValue +
          "/cutoff:" +
          cutoff +
          "/weight:" +
          weight +
          "/frecency:" +
          frecency
      );
      if (visitType == Ci.nsINavHistoryService.TRANSITION_BOOKMARK) {
        matchTitle = searchTerm + "Bookmarked";
        await PlacesUtils.bookmarks.insert({
          parentGuid: PlacesUtils.bookmarks.unfiledGuid,
          url: calculatedURI,
          title: matchTitle,
        });
      } else {
        matchTitle = calculatedURI.spec.substr(
          calculatedURI.spec.lastIndexOf("/") + 1
        );
      }
      await PlacesTestUtils.addVisits({
        uri: calculatedURI,
        transition: visitType,
        visitDate: dateInPeriod,
      });
    }

    if (calculatedURI && frecency) {
      results.push([calculatedURI, frecency, matchTitle]);
      await PlacesTestUtils.addVisits({
        uri: calculatedURI,
        title: matchTitle,
        transition: visitType,
        visitDate: dateInPeriod,
      });
    }
  }
}

add_task(async function test_frecency() {
  // Disable autoFill for this test.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  // always search in history + bookmarks, no matter what the default is
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmarks", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.history");
    Services.prefs.clearUserPref("browser.urlbar.suggest.bookmarks");
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });
  for (let bucket of bucketPrefs) {
    await task_initializeBucket(bucket);
  }

  // Sort results by frecency. Break ties by alphabetical URL.
  results.sort((a, b) => {
    let frecencyDiff = b[1] - a[1];
    if (frecencyDiff == 0) {
      return a[0].spec.localeCompare(b[0].spec);
    }
    return frecencyDiff;
  });

  // Make sure there's enough results returned
  Services.prefs.setIntPref(
    "browser.urlbar.maxRichResults",
    // +1 for the heuristic search result.
    results.length + 1
  );

  await PlacesTestUtils.promiseAsyncUpdates();
  let context = createContext(searchTerm, { isPrivate: false });
  let urlbarResults = [];
  for (let result of results) {
    let url = result[0].spec;
    if (url.toLowerCase().includes("bookmark")) {
      urlbarResults.push(
        makeBookmarkResult(context, {
          uri: url,
          title: result[2],
        })
      );
    } else {
      urlbarResults.push(
        makeVisitResult(context, {
          uri: url,
          title: result[2],
        })
      );
    }
  }

  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      ...urlbarResults,
    ],
  });
});
