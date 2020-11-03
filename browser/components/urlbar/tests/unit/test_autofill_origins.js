/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ENGINE_NAME = "engine-suggestions.xml";
const HEURISTIC_FALLBACK_PROVIDERNAME = "HeuristicFallback";

const origin = "example.com";

async function cleanup() {
  let suggestPrefs = ["history", "bookmark", "openpage"];
  for (let type of suggestPrefs) {
    Services.prefs.clearUserPref("browser.urlbar.suggest." + type);
  }
  await cleanupPlaces();
}

testEngine_setup();

// "example.com/" should match http://example.com/.  i.e., the search string
// should be treated as if it didn't have the trailing slash.
add_task(async function trailingSlash() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/",
    },
  ]);

  let context = createContext(`${origin}/`, { isPrivate: false });
  await check_results({
    context,
    autofilled: `${origin}/`,
    completed: `http://${origin}/`,
    matches: [
      makeVisitResult(context, {
        uri: `http://${origin}/`,
        title: `${origin}/`,
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "example.com/" should match http://www.example.com/.  i.e., the search string
// should be treated as if it didn't have the trailing slash.
add_task(async function trailingSlashWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://www.example.com/",
    },
  ]);
  let context = createContext(`${origin}/`, { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/",
    completed: "http://www.example.com/",
    matches: [
      makeVisitResult(context, {
        uri: `http://www.${origin}/`,
        title: `www.${origin}/`,
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "ex" should match http://example.com:8888/, and the port should be completed.
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/",
    },
  ]);
  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/",
    completed: "http://example.com:8888/",
    matches: [
      makeVisitResult(context, {
        uri: `http://${origin}:8888/`,
        title: `${origin}:8888`,
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "example.com:8" should match http://example.com:8888/, and the port should
// be completed.
add_task(async function portPartial() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/",
    },
  ]);
  let context = createContext(`${origin}:8`, { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/",
    completed: "http://example.com:8888/",
    matches: [
      makeVisitResult(context, {
        uri: `http://${origin}:8888/`,
        title: `${origin}:8888`,
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "EXaM" should match http://example.com/ and the case of the search string
// should be preserved in the autofilled value.
add_task(async function preserveCase() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/",
    },
  ]);
  let context = createContext("EXaM", { isPrivate: false });
  await check_results({
    context,
    autofilled: "EXaMple.com/",
    completed: "http://example.com/",
    matches: [
      makeVisitResult(context, {
        uri: `http://${origin}/`,
        title: `${origin}`,
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "EXaM" should match http://example.com:8888/, the port should be completed,
// and the case of the search string should be preserved in the autofilled
// value.
add_task(async function preserveCasePort() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/",
    },
  ]);
  let context = createContext("EXaM", { isPrivate: false });
  await check_results({
    context,
    autofilled: "EXaMple.com:8888/",
    completed: "http://example.com:8888/",
    matches: [
      makeVisitResult(context, {
        uri: `http://${origin}:8888/`,
        title: `${origin}:8888`,
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "example.com:89" should *not* match http://example.com:8888/.
add_task(async function portNoMatch1() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/",
    },
  ]);
  let context = createContext(`${origin}:89`, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${origin}:89/`,
        title: `http://${origin}:89/`,
        iconUri: "",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// "example.com:9" should *not* match http://example.com:8888/.
add_task(async function portNoMatch2() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/",
    },
  ]);
  let context = createContext(`${origin}:9`, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${origin}:9/`,
        title: `http://${origin}:9/`,
        iconUri: "",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// "example/" should *not* match http://example.com/.
add_task(async function trailingSlash_2() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/",
    },
  ]);
  let context = createContext("example/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://example/",
        title: "http://example/",
        iconUri: "page-icon:http://example/",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// multi.dotted.domain, search up to dot.
add_task(async function multidotted() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://www.example.co.jp:8888/",
    },
  ]);
  let context = createContext("www.example.co.", { isPrivate: false });
  await check_results({
    context,
    autofilled: "www.example.co.jp:8888/",
    completed: "http://www.example.co.jp:8888/",
    matches: [
      makeVisitResult(context, {
        uri: "http://www.example.co.jp:8888/",
        title: "www.example.co.jp:8888",
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

add_task(async function test_ip() {
  // IP addresses have complicated rules around whether they show
  // HeuristicFallback's backup search result. Flip this pref to disable that
  // backup search and simplify ths subtest.
  Services.prefs.setBoolPref("keyword.enabled", false);
  for (let str of [
    "192.168.1.1/",
    "255.255.255.255:8080/",
    "[2001:db8::1428:57ab]/",
    "[::c0a8:5909]/",
    "[::1]/",
  ]) {
    info("testing " + str);
    await PlacesTestUtils.addVisits("http://" + str);
    for (let i = 1; i < str.length; ++i) {
      let context = createContext(str.substring(0, i), { isPrivate: false });
      await check_results({
        context,
        autofilled: str,
        completed: "http://" + str,
        matches: [
          makeVisitResult(context, {
            uri: "http://" + str,
            title: str.replace(/\/$/, ""), // strip trailing slash
            heuristic: true,
          }),
        ],
      });
    }
    await cleanup();
  }
  Services.prefs.clearUserPref("keyword.enabled");
});

// host starting with large number.
add_task(async function large_number_host() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://12345example.it:8888/",
    },
  ]);
  let context = createContext("1234", { isPrivate: false });
  await check_results({
    context,
    autofilled: "12345example.it:8888/",
    completed: "http://12345example.it:8888/",
    matches: [
      makeVisitResult(context, {
        uri: "http://12345example.it:8888/",
        title: "12345example.it:8888",
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// When determining which origins should be autofilled, all the origins sharing
// a host should be added together to get their combined frecency -- i.e.,
// prefixes should be collapsed.  And then from that list, the origin with the
// highest frecency should be chosen.
add_task(async function groupByHost() {
  // Add some visits to the same host, example.com.  Add one http and two https
  // so that https has a higher frecency and is therefore the origin that should
  // be autofilled.  Also add another origin that has a higher frecency than
  // both so that alone, neither http nor https would be autofilled, but added
  // together they should be.
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },

    { uri: "https://example.com/" },
    { uri: "https://example.com/" },

    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
  ]);

  let httpFrec = frecencyForUrl("http://example.com/");
  let httpsFrec = frecencyForUrl("https://example.com/");
  let otherFrec = frecencyForUrl("https://mozilla.org/");
  Assert.less(httpFrec, httpsFrec, "Sanity check");
  Assert.less(httpsFrec, otherFrec, "Sanity check");

  // Make sure the frecencies of the three origins are as expected in relation
  // to the threshold.
  let threshold = await getOriginAutofillThreshold();
  Assert.less(httpFrec, threshold, "http origin should be < threshold");
  Assert.less(httpsFrec, threshold, "https origin should be < threshold");
  Assert.ok(threshold <= otherFrec, "Other origin should cross threshold");

  Assert.ok(
    threshold <= httpFrec + httpsFrec,
    "http and https origin added together should cross threshold"
  );

  // The https origin should be autofilled.
  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/",
    completed: "https://example.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "https://example.com",
        heuristic: true,
      }),
    ],
  });

  await cleanup();
});

// This is the same as the previous (groupByHost), but it changes the standard
// deviation multiplier by setting the corresponding pref.  This makes sure that
// the pref is respected.
add_task(async function groupByHostNonDefaultStddevMultiplier() {
  let stddevMultiplier = 1.5;
  Services.prefs.setCharPref(
    "browser.urlbar.autoFill.stddevMultiplier",
    Number(stddevMultiplier).toFixed(1)
  );

  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://example.com/" },

    { uri: "https://example.com/" },
    { uri: "https://example.com/" },
    { uri: "https://example.com/" },

    { uri: "https://foo.com/" },
    { uri: "https://foo.com/" },
    { uri: "https://foo.com/" },

    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
  ]);

  let httpFrec = frecencyForUrl("http://example.com/");
  let httpsFrec = frecencyForUrl("https://example.com/");
  let otherFrec = frecencyForUrl("https://mozilla.org/");
  Assert.less(httpFrec, httpsFrec, "Sanity check");
  Assert.less(httpsFrec, otherFrec, "Sanity check");

  // Make sure the frecencies of the three origins are as expected in relation
  // to the threshold.
  let threshold = await getOriginAutofillThreshold();
  Assert.less(httpFrec, threshold, "http origin should be < threshold");
  Assert.less(httpsFrec, threshold, "https origin should be < threshold");
  Assert.ok(threshold <= otherFrec, "Other origin should cross threshold");

  Assert.ok(
    threshold <= httpFrec + httpsFrec,
    "http and https origin added together should cross threshold"
  );

  // The https origin should be autofilled.
  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/",
    completed: "https://example.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "https://example.com",
        heuristic: true,
      }),
    ],
  });

  Services.prefs.clearUserPref("browser.urlbar.autoFill.stddevMultiplier");

  await cleanup();
});

// This is similar to suggestHistoryFalse_bookmark_0 in test_autofill_tasks.js,
// but it adds unbookmarked visits for multiple URLs with the same origin.
add_task(async function suggestHistoryFalse_bookmark_multiple() {
  // Force only bookmarked pages to be suggested and therefore only bookmarked
  // pages to be completed.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);

  let search = "ex";
  let baseURL = "http://example.com/";
  let bookmarkedURL = baseURL + "bookmarked";

  // Add visits for three different URLs all sharing the same origin, and then
  // bookmark the second one.  After that, the origin should be autofilled.  The
  // reason for adding unbookmarked visits before and after adding the
  // bookmarked visit is to make sure our aggregate SQL query for determining
  // whether an origin is bookmarked is correct.

  await PlacesTestUtils.addVisits([
    {
      uri: baseURL + "other1",
    },
  ]);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        heuristic: true,
      }),
    ],
  });

  await PlacesTestUtils.addVisits([
    {
      uri: bookmarkedURL,
    },
  ]);
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        heuristic: true,
      }),
    ],
  });

  await PlacesTestUtils.addVisits([
    {
      uri: baseURL + "other2",
    },
  ]);
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        heuristic: true,
      }),
    ],
  });

  // Now bookmark the second URL.  It should be suggested and completed.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: bookmarkedURL,
  });
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/",
    completed: baseURL,
    matches: [
      makeVisitResult(context, {
        uri: baseURL,
        title: "example.com",
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: bookmarkedURL,
        title: "A bookmark",
      }),
    ],
  });

  await cleanup();
});

// This is similar to suggestHistoryFalse_bookmark_prefix_0 in
// autofill_tasks.js, but it adds unbookmarked visits for multiple URLs with the
// same origin.
add_task(async function suggestHistoryFalse_bookmark_prefix_multiple() {
  // Force only bookmarked pages to be suggested and therefore only bookmarked
  // pages to be completed.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);

  let search = "http://ex";
  let baseURL = "http://example.com/";
  let bookmarkedURL = baseURL + "bookmarked";

  // Add visits for three different URLs all sharing the same origin, and then
  // bookmark the second one.  After that, the origin should be autofilled.  The
  // reason for adding unbookmarked visits before and after adding the
  // bookmarked visit is to make sure our aggregate SQL query for determining
  // whether an origin is bookmarked is correct.

  await PlacesTestUtils.addVisits([
    {
      uri: baseURL + "other1",
    },
  ]);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${search}/`,
        title: `${search}/`,
        iconUri: "",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });

  await PlacesTestUtils.addVisits([
    {
      uri: bookmarkedURL,
    },
  ]);
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${search}/`,
        title: `${search}/`,
        iconUri: "",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });

  await PlacesTestUtils.addVisits([
    {
      uri: baseURL + "other2",
    },
  ]);
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${search}/`,
        title: `${search}/`,
        iconUri: "",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });

  // Now bookmark the second URL.  It should be suggested and completed.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: bookmarkedURL,
  });
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://example.com/",
    completed: baseURL,
    matches: [
      makeVisitResult(context, {
        uri: baseURL,
        title: "example.com",
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: bookmarkedURL,
        title: "A bookmark",
      }),
    ],
  });

  await cleanup();
});
