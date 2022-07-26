/**
 * Test for bug 1211726 - preload list of top web sites for better
 * autocompletion on empty profiles.
 */

testEngine_setup();

const PREF_FEATURE_ENABLED = "browser.urlbar.usepreloadedtopurls.enabled";
const PREF_FEATURE_EXPIRE_DAYS =
  "browser.urlbar.usepreloadedtopurls.expire_days";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderPreloadedSites:
    "resource:///modules/UrlbarProviderPreloadedSites.sys.mjs",
});

Cu.importGlobalProperties(["fetch"]);

let yahoooURI = "https://yahooo.com/";
let gooogleURI = "https://gooogle.com/";

UrlbarProviderPreloadedSites.populatePreloadedSiteStorage([
  [yahoooURI, "Yahooo"],
  [gooogleURI, "Gooogle"],
]);

async function assert_feature_works(condition) {
  info("List Results do appear " + condition);
  let context = createContext("ooo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        engineName: SUGGESTIONS_ENGINE_NAME,
        providerName: "HeuristicFallback",
      }),
      makeVisitResult(context, {
        uri: yahoooURI,
        title: "Yahooo",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: `page-icon:${yahoooURI}`,
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: gooogleURI,
        title: "Gooogle",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: `page-icon:${gooogleURI}`,
        providerName: "PreloadedSites",
      }),
    ],
  });

  info("Autofill does appear " + condition);
  context = createContext("gooo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "gooogle.com/",
    completed: gooogleURI,
    matches: [
      makeVisitResult(context, {
        uri: gooogleURI,
        title: gooogleURI.slice(0, -1), // Trim trailing slash.
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: `page-icon:${gooogleURI}`,
        providerName: "PreloadedSites",
        heuristic: true,
      }),
    ],
  });
}

async function assert_feature_does_not_appear(condition) {
  info("List Results don't appear " + condition);
  let context = createContext("ooo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        engineName: SUGGESTIONS_ENGINE_NAME,
        providerName: "HeuristicFallback",
      }),
    ],
  });

  info("Autofill doesn't appear " + condition);
  context = createContext("gooo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        engineName: SUGGESTIONS_ENGINE_NAME,
        providerName: "HeuristicFallback",
      }),
    ],
  });
}

add_task(async function test_it_works() {
  // Not expired but OFF
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);
  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, false);
  await assert_feature_does_not_appear("when OFF by prefs");

  // Now turn it ON
  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  await assert_feature_works("when ON by prefs");

  // And expire
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 0);
  await assert_feature_does_not_appear("when expired");

  await cleanupPlaces();
});

add_task(async function test_sorting_against_bookmark() {
  let boookmarkURI = "https://boookmark.com/";
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: boookmarkURI,
    title: "Boookmark",
  });

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  info("Preloaded Top Sites are placed lower than Bookmarks");
  let context = createContext("ooo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: boookmarkURI,
        title: "Boookmark",
      }),
      makeVisitResult(context, {
        uri: yahoooURI,
        title: "Yahooo",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: `page-icon:${yahoooURI}`,
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: gooogleURI,
        title: "Gooogle",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: `page-icon:${gooogleURI}`,
        providerName: "PreloadedSites",
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_sorting_against_history() {
  let histoooryURI = "https://histooory.com/";
  await PlacesTestUtils.addVisits({ uri: histoooryURI, title: "Histooory" });

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  info("Preloaded Top Sites are placed lower than History entries");
  let context = createContext("ooo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: histoooryURI,
        title: "Histooory",
      }),
      makeVisitResult(context, {
        uri: yahoooURI,
        title: "Yahooo",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: `page-icon:${yahoooURI}`,
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: gooogleURI,
        title: "Gooogle",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: `page-icon:${gooogleURI}`,
        providerName: "PreloadedSites",
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_scheme_and_www() {
  // Order is important to check sorting
  let sites = [
    ["https://www.ooops-https-www.com/", "Ooops"],
    ["https://ooops-https.com/", "Ooops"],
    ["HTTP://ooops-HTTP.com/", "Ooops"],
    ["HTTP://www.ooops-HTTP-www.com/", "Ooops"],
    ["https://foo.com/", "Title with www"],
    ["https://www.bar.com/", "Tile"],
  ];

  let titlesMap = new Map(sites);

  UrlbarProviderPreloadedSites.populatePreloadedSiteStorage(sites);

  // No matches when just typing the protocol.
  let context = createContext("https://", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  context = createContext("www.", { isPrivate: false });
  await check_results({
    context,
    autofilled: "www.ooops-https-www.com/",
    completed: "https://www.ooops-https-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.ooops-https-www.com/",
        title: "https://www.ooops-https-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:https://www.ooops-https-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://www.ooops-http-www.com/",
        title: titlesMap.get("HTTP://www.ooops-HTTP-www.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:http://www.ooops-http-www.com/",
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: "https://www.bar.com/",
        title: titlesMap.get("https://www.bar.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:https://www.bar.com/",
        providerName: "PreloadedSites",
      }),
    ],
  });

  context = createContext("http://www.", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://www.ooops-http-www.com/",
    completed: "http://www.ooops-http-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "http://www.ooops-http-www.com/",
        title: "www.ooops-http-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:http://www.ooops-http-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
    ],
  });

  context = createContext("ftp://ooops", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "ftp://ooops/",
        title: "ftp://ooops/",
        providerName: "HeuristicFallback",
        heuristic: true,
      }),
    ],
  });

  context = createContext("ww", { isPrivate: false });
  await check_results({
    context,
    autofilled: "www.ooops-https-www.com/",
    completed: "https://www.ooops-https-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.ooops-https-www.com/",
        title: "https://www.ooops-https-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:https://www.ooops-https-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://www.ooops-http-www.com/",
        title: titlesMap.get("HTTP://www.ooops-HTTP-www.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:http://www.ooops-http-www.com/",
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: "https://foo.com/",
        title: titlesMap.get("https://foo.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:https://foo.com/",
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: "https://www.bar.com/",
        title: titlesMap.get("https://www.bar.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:https://www.bar.com/",
        providerName: "PreloadedSites",
      }),
    ],
  });

  context = createContext("ooops", { isPrivate: false });
  await check_results({
    context,
    autofilled: "ooops-https-www.com/",
    completed: "https://www.ooops-https-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.ooops-https-www.com/",
        title: "https://www.ooops-https-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:https://www.ooops-https-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://ooops-https.com/",
        title: titlesMap.get("https://ooops-https.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:https://ooops-https.com/",
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: "http://ooops-http.com/",
        title: titlesMap.get("HTTP://ooops-HTTP.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:http://ooops-http.com/",
        providerName: "PreloadedSites",
      }),
      makeVisitResult(context, {
        uri: "http://www.ooops-http-www.com/",
        title: titlesMap.get("HTTP://www.ooops-HTTP-www.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:http://www.ooops-http-www.com/",
        providerName: "PreloadedSites",
      }),
    ],
  });

  context = createContext("www.ooops", { isPrivate: false });
  await check_results({
    context,
    autofilled: "www.ooops-https-www.com/",
    completed: "https://www.ooops-https-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.ooops-https-www.com/",
        title: "https://www.ooops-https-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:https://www.ooops-https-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://www.ooops-http-www.com/",
        title: titlesMap.get("HTTP://www.ooops-HTTP-www.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:http://www.ooops-http-www.com/",
        providerName: "PreloadedSites",
      }),
    ],
  });

  context = createContext("ooops-https-www", { isPrivate: false });
  await check_results({
    context,
    autofilled: "ooops-https-www.com/",
    completed: "https://www.ooops-https-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.ooops-https-www.com/",
        title: "https://www.ooops-https-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:https://www.ooops-https-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
    ],
  });

  context = createContext("www.ooops-https.", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://www.ooops-https./",
        title: "http://www.ooops-https./",
        providerName: "HeuristicFallback",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        providerName: "HeuristicFallback",
      }),
    ],
  });

  context = createContext("https://ooops", { isPrivate: false });
  await check_results({
    context,
    autofilled: "https://ooops-https-www.com/",
    completed: "https://www.ooops-https-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.ooops-https-www.com/",
        title: "https://www.ooops-https-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:https://www.ooops-https-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://ooops-https.com/",
        title: titlesMap.get("https://ooops-https.com/"),
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        tags: null,
        iconUri: "page-icon:https://ooops-https.com/",
        providerName: "PreloadedSites",
      }),
    ],
  });

  context = createContext("https://www.ooops", { isPrivate: false });
  await check_results({
    context,
    autofilled: "https://www.ooops-https-www.com/",
    completed: "https://www.ooops-https-www.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.ooops-https-www.com/",
        title: "https://www.ooops-https-www.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: "page-icon:https://www.ooops-https-www.com/",
        providerName: "PreloadedSites",
        heuristic: true,
      }),
    ],
  });

  context = createContext("http://www.ooops-http.", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://www.ooops-http./",
        title: "http://www.ooops-http./",
        providerName: "HeuristicFallback",
        heuristic: true,
      }),
    ],
  });

  context = createContext("http://ooops-https", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://ooops-https/",
        title: "http://ooops-https/",
        providerName: "HeuristicFallback",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_data_file() {
  let response = await fetch(
    "chrome://browser/content/urlbar/preloaded-top-urls.json"
  );

  info("Source file is supplied and fetched OK");
  Assert.ok(response.ok);

  info("The JSON is parsed");
  let sites = await response.json();

  // Add test site so this test doesn't depend on the contents of the data file.
  sites.push(["https://www.example.com/", "Example"]);

  info("Storage is populated");
  UrlbarProviderPreloadedSites.populatePreloadedSiteStorage(sites);

  let lastSite = sites.pop();
  let uri = Services.io.newURI(lastSite[0]);

  info("Storage is populated from JSON correctly");
  let context = createContext(uri.host, { isPrivate: false });
  await check_results({
    context,
    autofilled: uri.host + "/",
    completed: uri.spec,
    matches: [
      makeVisitResult(context, {
        uri: uri.spec,
        title: uri.spec.slice(0, -1), // Trim trailing slash.
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: `page-icon:${uri.spec}`,
        providerName: "PreloadedSites",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_partial_scheme() {
  // "tt" should not result in a match of "ttps://whatever.com/".
  let testUrl = "http://www.ttt.com/";
  UrlbarProviderPreloadedSites.populatePreloadedSiteStorage([
    [testUrl, "Test"],
  ]);
  let context = createContext("tt", { isPrivate: false });
  await check_results({
    context,
    autofilled: "ttt.com/",
    completed: testUrl,
    matches: [
      makeVisitResult(context, {
        uri: testUrl,
        title: "www.ttt.com",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        iconUri: `page-icon:${testUrl}`,
        heuristic: true,
        providerName: "PreloadedSites",
      }),
    ],
  });
  await cleanupPlaces();
});
