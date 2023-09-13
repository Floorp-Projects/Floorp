/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const HEURISTIC_FALLBACK_PROVIDERNAME = "HeuristicFallback";
const PLACES_PROVIDERNAME = "Places";

/**
 * Helpful reminder of the `autofilled` and `completed` properties in the
 * object passed to check_results:
 *  autofilled: expected input.value after autofill
 *  completed: expected input.value after autofill and enter is pressed
 *
 * `completed` is the URL that the controller sets to input.value, and the URL
 * that will ultimately be loaded when you press enter.
 */

async function cleanup() {
  let suggestPrefs = ["history", "bookmark", "openpage"];
  for (let type of suggestPrefs) {
    Services.prefs.clearUserPref("browser.urlbar.suggest." + type);
  }
  await cleanupPlaces();
}

testEngine_setup();

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
});
Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

let path;
let search;
let searchCase;
let visitTitle;
let url;
const host = "example.com";
let origins;

function add_autofill_task(callback) {
  let func = async () => {
    info(`Running subtest with origins disabled: ${callback.name}`);
    origins = false;
    path = "/foo";
    search = "example.com/f";
    searchCase = "EXAMPLE.COM/f";
    visitTitle = (protocol, sub) =>
      `test visit for ${protocol}://${sub}example.com/foo`;
    url = host + path;
    await callback();

    info(`Running subtest with origins enabled: ${callback.name}`);
    origins = true;
    path = "/";
    search = "ex";
    searchCase = "EX";
    visitTitle = (protocol, sub) =>
      `test visit for ${protocol}://${sub}example.com/`;
    url = host + path;
    await callback();
  };
  Object.defineProperty(func, "name", { value: callback.name });
  add_task(func);
}

// "ex" should match http://example.com/.
add_autofill_task(async function basic() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "EX" should match http://example.com/.
add_autofill_task(async function basicCase() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext(searchCase, { isPrivate: false });
  await check_results({
    context,
    autofilled: searchCase + url.substr(searchCase.length),
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "ex" should match http://www.example.com/.
add_autofill_task(async function noWWWShouldMatchWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://www." + url,
    },
  ]);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://www." + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://www." + url,
        title: visitTitle("http", "www."),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "EX" should match http://www.example.com/.
add_autofill_task(async function noWWWShouldMatchWWWCase() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://www." + url,
    },
  ]);
  let context = createContext(searchCase, { isPrivate: false });
  await check_results({
    context,
    autofilled: searchCase + url.substr(searchCase.length),
    completed: "http://www." + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://www." + url,
        title: visitTitle("http", "www."),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "www.ex" should *not* match http://example.com/.
add_autofill_task(async function wwwShouldNotMatchNoWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext("www." + search, { isPrivate: false });
  if (origins) {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://www." + search + "/",
          fallbackTitle: "http://www." + search + "/",
          displayUrl: "http://www." + search,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  } else {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://www." + search,
          fallbackTitle: "http://www." + search,
          iconUri: `page-icon:http://www.${host}/`,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  }
  await cleanup();
});

// "http://ex" should match http://example.com/.
add_autofill_task(async function prefix() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext("http://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://" + url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "HTTP://EX" should match http://example.com/.
add_autofill_task(async function prefixCase() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext("HTTP://" + searchCase, { isPrivate: false });
  await check_results({
    context,
    autofilled: "HTTP://" + searchCase + url.substr(searchCase.length),
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "http://ex" should match http://www.example.com/.
add_autofill_task(async function prefixNoWWWShouldMatchWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://www." + url,
    },
  ]);
  let context = createContext("http://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://" + url,
    completed: "http://www." + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://www." + url,
        title: visitTitle("http", "www."),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "HTTP://EX" should match http://www.example.com/.
add_autofill_task(async function prefixNoWWWShouldMatchWWWCase() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://www." + url,
    },
  ]);
  let context = createContext("HTTP://" + searchCase, { isPrivate: false });
  await check_results({
    context,
    autofilled: "HTTP://" + searchCase + url.substr(searchCase.length),
    completed: "http://www." + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://www." + url,
        title: visitTitle("http", "www."),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "http://www.ex" should *not* match http://example.com/.
add_autofill_task(async function prefixWWWShouldNotMatchNoWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext("http://www." + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://www.${search}/` : `http://www.${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://www.${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// "http://ex" should *not* match https://example.com/.
add_autofill_task(async function httpPrefixShouldNotMatchHTTPS() {
  await PlacesTestUtils.addVisits([
    {
      uri: "https://" + url,
    },
  ]);
  let context = createContext("http://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "https://" + url,
        title: "test visit for https://" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// "ex" should match https://example.com/.
add_autofill_task(async function httpsBasic() {
  await PlacesTestUtils.addVisits([
    {
      uri: "https://" + url,
    },
  ]);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "https://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://" + url,
        title: visitTitle("https", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "ex" should match https://www.example.com/.
add_autofill_task(async function httpsNoWWWShouldMatchWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "https://www." + url,
    },
  ]);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "https://www." + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://www." + url,
        title: visitTitle("https", "www."),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "www.ex" should *not* match https://example.com/.
add_autofill_task(async function httpsWWWShouldNotMatchNoWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "https://" + url,
    },
  ]);
  let context = createContext("www." + search, { isPrivate: false });
  if (origins) {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://www." + search + "/",
          fallbackTitle: "http://www." + search + "/",
          displayUrl: "http://www." + search,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  } else {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://www." + search,
          fallbackTitle: "http://www." + search,
          iconUri: `page-icon:http://www.${host}/`,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  }
  await cleanup();
});

// "https://ex" should match https://example.com/.
add_autofill_task(async function httpsPrefix() {
  await PlacesTestUtils.addVisits([
    {
      uri: "https://" + url,
    },
  ]);
  let context = createContext("https://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "https://" + url,
    completed: "https://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://" + url,
        title: visitTitle("https", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "https://ex" should match https://www.example.com/.
add_autofill_task(async function httpsPrefixNoWWWShouldMatchWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "https://www." + url,
    },
  ]);
  let context = createContext("https://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "https://" + url,
    completed: "https://www." + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://www." + url,
        title: visitTitle("https", "www."),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// "https://www.ex" should *not* match https://example.com/.
add_autofill_task(async function httpsPrefixWWWShouldNotMatchNoWWW() {
  await PlacesTestUtils.addVisits([
    {
      uri: "https://" + url,
    },
  ]);
  let context = createContext("https://www." + search, { isPrivate: false });
  let prefixedUrl = origins
    ? `https://www.${search}/`
    : `https://www.${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:https://www.${host}/`,
        providerame: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// "https://ex" should *not* match http://example.com/.
add_autofill_task(async function httpsPrefixShouldNotMatchHTTP() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext("https://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `https://${search}/` : `https://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:https://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "test visit for http://" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// "https://ex" should *not* match http://example.com/, even if the latter is
// more frecent and both could be autofilled.
add_autofill_task(async function httpsPrefixShouldNotMatchMoreFrecentHTTP() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
    {
      uri: "http://" + url,
    },
    {
      uri: "https://" + url,
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
    {
      uri: "http://otherpage",
    },
  ]);
  let context = createContext("https://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "https://" + url,
    completed: "https://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://" + url,
        title: visitTitle("https", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// Autofill should respond to frecency changes.
add_autofill_task(async function frecency() {
  // Start with an http visit.  It should be completed.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://" + url,
    },
  ]);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });

  // Add two https visits.  https should now be completed.
  for (let i = 0; i < 2; i++) {
    await PlacesTestUtils.addVisits([{ uri: "https://" + url }]);
  }
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "https://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://" + url,
        title: visitTitle("https", ""),
        heuristic: true,
      }),
    ],
  });

  // Add two more http visits, three total.  http should now be completed
  // again.
  for (let i = 0; i < 2; i++) {
    await PlacesTestUtils.addVisits([{ uri: "http://" + url }]);
  }
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://" + url,
        title: "test visit for https://" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });

  // Add four www https visits.  www https should now be completed.
  for (let i = 0; i < 4; i++) {
    await PlacesTestUtils.addVisits([{ uri: "https://www." + url }]);
  }
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "https://www." + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://www." + url,
        title: visitTitle("https", "www."),
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://" + url,
        title: "test visit for https://" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });

  // Remove the www https page.
  await PlacesUtils.history.remove(["https://www." + url]);

  // http should now be completed again.
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://" + url,
        title: "test visit for https://" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });

  // Remove the http page.
  await PlacesUtils.history.remove(["http://" + url]);

  // https should now be completed again.
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "https://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://" + url,
        title: visitTitle("https", ""),
        heuristic: true,
      }),
    ],
  });

  // Add a visit with a different host so that "ex" doesn't autofill it.
  // https://example.com/ should still have a higher frecency though, so it
  // should still be autofilled.
  await PlacesTestUtils.addVisits([{ uri: "https://not-" + url }]);
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "https://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://" + url,
        title: visitTitle("https", ""),
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://not-" + url,
        title: "test visit for https://not-" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });

  // Now add 10 more visits to the different host so that the frecency of
  // https://example.com/ falls below the autofill threshold.  It should not
  // be autofilled now.
  for (let i = 0; i < 10; i++) {
    await PlacesTestUtils.addVisits([{ uri: "https://not-" + url }]);
  }

  // In the `origins` case, the failure to make an autofill match means
  // HeuristicFallback should not create a heuristic result. In the
  // `!origins` case, autofill should still happen since there's no threshold
  // comparison.
  context = createContext(search, { isPrivate: false });
  if (origins) {
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "https://not-" + url,
          title: "test visit for https://not-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "https://" + url,
          title: "test visit for https://" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
  } else {
    await check_results({
      context,
      autofilled: url,
      completed: "https://" + url,
      matches: [
        makeVisitResult(context, {
          uri: "https://" + url,
          title: visitTitle("https", ""),
          heuristic: true,
        }),
        makeVisitResult(context, {
          uri: "https://not-" + url,
          title: "test visit for https://not-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
  }

  // Remove the visits to the different host.
  await PlacesUtils.history.remove(["https://not-" + url]);

  // https should be completed again.
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "https://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "https://" + url,
        title: visitTitle("https", ""),
        heuristic: true,
      }),
    ],
  });

  // Remove the https visits.
  await PlacesUtils.history.remove(["https://" + url]);

  // Now nothing should be completed.
  context = createContext(search, { isPrivate: false });
  if (origins) {
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  } else {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://" + search,
          fallbackTitle: "http://" + search,
          iconUri: `page-icon:http://${host}/`,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  }

  await cleanup();
});

// Bookmarked places should always be autofilled, even when they don't meet
// the threshold.
add_autofill_task(async function bookmarkBelowThreshold() {
  // Add some visits to a URL so that the origin autofill threshold is large.
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://not-" + url,
      },
    ]);
  }

  // Now bookmark another URL.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  // Make sure the bookmarked origin and place frecencies are below the
  // threshold so that the origin/URL otherwise would not be autofilled.
  let placeFrecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    { url: "http://" + url }
  );
  let originFrecency = await getOriginFrecency("http://", host);
  let threshold = await getOriginAutofillThreshold();
  Assert.ok(
    placeFrecency < threshold,
    `Place frecency should be below the threshold: ` +
      `placeFrecency=${placeFrecency} threshold=${threshold}`
  );
  Assert.ok(
    originFrecency < threshold,
    `Origin frecency should be below the threshold: ` +
      `originFrecency=${originFrecency} threshold=${threshold}`
  );

  // The bookmark should be autofilled.
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "A bookmark",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://not-" + url,
        title: "test visit for http://not-" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });

  await cleanup();
});

// Bookmarked places should be autofilled when they *do* meet the threshold.
add_autofill_task(async function bookmarkAboveThreshold() {
  // Bookmark a URL.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  // The frecencies of the place and origin should be >= the threshold.  In
  // fact they should be the same as the threshold since the place is the only
  // place in the database.
  let placeFrecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    { url: "http://" + url }
  );
  let originFrecency = await getOriginFrecency("http://", host);
  let threshold = await getOriginAutofillThreshold();
  Assert.equal(placeFrecency, threshold);
  Assert.equal(originFrecency, threshold);

  // The bookmark should be autofilled.
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "A bookmark",
        heuristic: true,
      }),
    ],
  });

  await cleanup();
});

// Bookmark a page and then clear history.
// The bookmarked origin/URL should still be autofilled.
add_autofill_task(async function zeroThreshold() {
  const pageUrl = "http://" + url;
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: pageUrl,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  await PlacesUtils.history.clear();
  await PlacesUtils.withConnectionWrapper("zeroThreshold", async db => {
    await db.execute("UPDATE moz_places SET frecency = -1 WHERE url = :url", {
      url: pageUrl,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  });

  // Make sure the place's frecency is -1.
  let placeFrecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    { url: pageUrl }
  );
  Assert.equal(placeFrecency, -1);

  // Make sure the origin's frecency is 0.
  let originFrecency = await getOriginFrecency("http://", host);
  Assert.equal(originFrecency, 0);

  // Make sure the autofill threshold is 0.
  let threshold = await getOriginAutofillThreshold();
  Assert.equal(threshold, 0);

  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "A bookmark",
        heuristic: true,
      }),
    ],
  });

  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: visit
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestHistoryFalse_visit() {
  await PlacesTestUtils.addVisits("http://" + url);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  context = createContext(search, { isPrivate: false });
  if (origins) {
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  } else {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://" + search,
          fallbackTitle: "http://" + search,
          iconUri: `page-icon:http://${host}/`,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  }
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: visit
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestHistoryFalse_visit_prefix() {
  await PlacesTestUtils.addVisits("http://" + url);
  let context = createContext("http://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://" + url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  context = createContext(search, { isPrivate: false });
  if (origins) {
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  } else {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://" + search,
          fallbackTitle: "http://" + search,
          iconUri: `page-icon:http://${host}/`,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  }
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: bookmark
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: yes
//
// Expected result:
//   should autofill: yes
add_autofill_task(async function suggestHistoryFalse_bookmark_0() {
  // Add the bookmark.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  // Make the bookmark fall below the autofill frecency threshold so we ensure
  // the bookmark is always autofilled in this case, even if it doesn't meet
  // the threshold.
  await TestUtils.waitForCondition(async () => {
    // Add a visit to another origin to boost the threshold.
    await PlacesTestUtils.addVisits("http://foo-" + url);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    let originFrecency = await getOriginFrecency("http://", host);
    let threshold = await getOriginAutofillThreshold();
    return threshold > originFrecency;
  }, "Make the bookmark fall below the frecency threshold");

  // At this point, the bookmark doesn't meet the threshold, but it should
  // still be autofilled.
  let originFrecency = await getOriginFrecency("http://", host);
  let threshold = await getOriginAutofillThreshold();
  Assert.ok(originFrecency < threshold);

  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "A bookmark",
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: bookmark
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestHistoryFalse_bookmark_1() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://non-matching-" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let context = createContext(search, { isPrivate: false });
  let matches = [
    makeBookmarkResult(context, {
      uri: "http://non-matching-" + url,
      title: "A bookmark",
    }),
  ];
  if (origins) {
    matches.unshift(
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      })
    );
  } else {
    matches.unshift(
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://" + search,
        fallbackTitle: "http://" + search,
        iconUri: `page-icon:http://${host}/`,
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      })
    );
  }
  await check_results({
    context,
    matches,
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: bookmark
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: yes
//
// Expected result:
//   should autofill: yes
add_autofill_task(async function suggestHistoryFalse_bookmark_prefix_0() {
  // Add the bookmark.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  // Make the bookmark fall below the autofill frecency threshold so we ensure
  // the bookmark is always autofilled in this case, even if it doesn't meet
  // the threshold.
  await TestUtils.waitForCondition(async () => {
    // Add a visit to another origin to boost the threshold.
    await PlacesTestUtils.addVisits("http://foo-" + url);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    let originFrecency = await getOriginFrecency("http://", host);
    let threshold = await getOriginAutofillThreshold();
    return threshold > originFrecency;
  }, "Make the bookmark fall below the frecency threshold");

  // At this point, the bookmark doesn't meet the threshold, but it should
  // still be autofilled.
  let originFrecency = await getOriginFrecency("http://", host);
  let threshold = await getOriginAutofillThreshold();
  Assert.ok(originFrecency < threshold);

  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let context = createContext("http://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://" + url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "A bookmark",
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: bookmark
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestHistoryFalse_bookmark_prefix_1() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "ftp://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let context = createContext("http://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeBookmarkResult(context, {
        uri: "ftp://" + url,
        title: "A bookmark",
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: bookmark
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestHistoryFalse_bookmark_prefix_2() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://non-matching-" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let context = createContext("http://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeBookmarkResult(context, {
        uri: "http://non-matching-" + url,
        title: "A bookmark",
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = false
//   suggest.bookmark = true
//   search for: bookmark
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestHistoryFalse_bookmark_prefix_3() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "ftp://non-matching-" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let context = createContext("http://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeBookmarkResult(context, {
        uri: "ftp://non-matching-" + url,
        title: "A bookmark",
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visit
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: yes
//
// Expected result:
//   should autofill: yes
add_autofill_task(async function suggestBookmarkFalse_visit_0() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  await PlacesTestUtils.addVisits("http://" + url);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visit
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestBookmarkFalse_visit_1() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  await PlacesTestUtils.addVisits("http://non-matching-" + url);
  let context = createContext(search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  let matches = [
    makeVisitResult(context, {
      uri: "http://non-matching-" + url,
      title: "test visit for http://non-matching-" + url,
      providerName: PLACES_PROVIDERNAME,
    }),
  ];
  if (origins) {
    matches.unshift(
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      })
    );
  } else {
    matches.unshift(
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      })
    );
  }
  await check_results({
    context,
    matches,
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visit
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: yes
//
// Expected result:
//   should autofill: yes
add_autofill_task(async function suggestBookmarkFalse_visit_prefix_0() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  await PlacesTestUtils.addVisits("http://" + url);
  let context = createContext("http://" + search, { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://" + url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visit
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestBookmarkFalse_visit_prefix_1() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  await PlacesTestUtils.addVisits("ftp://" + url);
  let context = createContext("http://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "ftp://" + url,
        title: "test visit for ftp://" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visit
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestBookmarkFalse_visit_prefix_2() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  await PlacesTestUtils.addVisits("http://non-matching-" + url);
  let context = createContext("http://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "http://non-matching-" + url,
        title: "test visit for http://non-matching-" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visit
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestBookmarkFalse_visit_prefix_3() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  await PlacesTestUtils.addVisits("ftp://non-matching-" + url);
  let context = createContext("http://" + search, { isPrivate: false });
  let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: prefixedUrl,
        fallbackTitle: prefixedUrl,
        heuristic: true,
        iconUri: origins ? "" : `page-icon:http://${host}/`,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "ftp://non-matching-" + url,
        title: "test visit for ftp://non-matching-" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: unvisited bookmark
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestBookmarkFalse_unvisitedBookmark() {
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "A bookmark",
        heuristic: true,
      }),
    ],
  });
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  context = createContext(search, { isPrivate: false });
  if (origins) {
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  } else {
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: "http://" + search,
          fallbackTitle: "http://" + search,
          iconUri: `page-icon:http://${host}/`,
          heuristic: true,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
  }
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: unvisited bookmark
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_unvisitedBookmark_prefix_0() {
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "http://" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    let context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      autofilled: "http://" + url,
      completed: "http://" + url,
      matches: [
        makeVisitResult(context, {
          uri: "http://" + url,
          title: "A bookmark",
          heuristic: true,
        }),
      ],
    });
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    context = createContext("http://" + search, { isPrivate: false });
    let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: prefixedUrl,
          fallbackTitle: prefixedUrl,
          heuristic: true,
          iconUri: origins ? "" : `page-icon:http://${host}/`,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: unvisited bookmark
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_unvisitedBookmark_prefix_1() {
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "ftp://" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    let context = createContext("http://" + search, { isPrivate: false });
    let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: prefixedUrl,
          fallbackTitle: prefixedUrl,
          heuristic: true,
          iconUri: origins ? "" : `page-icon:http://${host}/`,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: unvisited bookmark
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_unvisitedBookmark_prefix_2() {
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "http://non-matching-" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    let context = createContext("http://" + search, { isPrivate: false });
    let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: prefixedUrl,
          fallbackTitle: prefixedUrl,
          heuristic: true,
          iconUri: origins ? "" : `page-icon:http://${host}/`,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: unvisited bookmark
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_unvisitedBookmark_prefix_3() {
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "ftp://non-matching-" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    let context = createContext("http://" + search, { isPrivate: false });
    let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: prefixedUrl,
          fallbackTitle: prefixedUrl,
          heuristic: true,
          iconUri: origins ? "" : `page-icon:http://${host}/`,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark above autofill threshold
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: yes
//
// Expected result:
//   should autofill: yes
add_autofill_task(async function suggestBookmarkFalse_visitedBookmark_above() {
  await PlacesTestUtils.addVisits("http://" + url);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark above autofill threshold
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: yes
//
// Expected result:
//   should autofill: yes
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkAbove_prefix_0() {
    await PlacesTestUtils.addVisits("http://" + url);
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "http://" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    let context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      autofilled: "http://" + url,
      completed: "http://" + url,
      matches: [
        makeVisitResult(context, {
          uri: "http://" + url,
          title: visitTitle("http", ""),
          heuristic: true,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark above autofill threshold
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkAbove_prefix_1() {
    await PlacesTestUtils.addVisits("ftp://" + url);
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "ftp://" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    let context = createContext("http://" + search, { isPrivate: false });
    let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: prefixedUrl,
          fallbackTitle: prefixedUrl,
          heuristic: true,
          iconUri: origins ? "" : `page-icon:http://${host}/`,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeBookmarkResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
          uri: "ftp://" + url,
          title: "A bookmark",
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark above autofill threshold
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkAbove_prefix_2() {
    await PlacesTestUtils.addVisits("http://non-matching-" + url);
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "http://non-matching-" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    let context = createContext("http://" + search, { isPrivate: false });
    let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: prefixedUrl,
          fallbackTitle: prefixedUrl,
          heuristic: true,
          iconUri: origins ? "" : `page-icon:http://${host}/`,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeBookmarkResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
          uri: "http://non-matching-" + url,
          title: "A bookmark",
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark above autofill threshold
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkAbove_prefix_3() {
    await PlacesTestUtils.addVisits("ftp://non-matching-" + url);
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "ftp://non-matching-" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    let context = createContext("http://" + search, { isPrivate: false });
    let prefixedUrl = origins ? `http://${search}/` : `http://${search}`;
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: prefixedUrl,
          fallbackTitle: prefixedUrl,
          heuristic: true,
          iconUri: origins ? "" : `page-icon:http://${host}/`,
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeBookmarkResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
          uri: "ftp://non-matching-" + url,
          title: "A bookmark",
        }),
      ],
    });
    await cleanup();
  }
);

// The following suggestBookmarkFalse_visitedBookmarkBelow* tests are similar
// to the suggestBookmarkFalse_visitedBookmarkAbove* tests, but instead of
// checking visited bookmarks above the autofill threshold, they check visited
// bookmarks below the threshold.  These tests don't make sense for URL
// queries (as opposed to origin queries) because URL queries don't use the
// same autofill threshold, so we skip them when !origins.

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark below autofill threshold
//   prefix search: no
//   prefix matches search: n/a
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(async function suggestBookmarkFalse_visitedBookmarkBelow() {
  if (!origins) {
    // See comment above suggestBookmarkFalse_visitedBookmarkBelow.
    return;
  }
  // First, make sure that `url` is below the autofill threshold.
  await PlacesTestUtils.addVisits("http://" + url);
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits("http://some-other-" + url);
  }
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "http://some-other-" + url,
        title: "test visit for http://some-other-" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "test visit for http://" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  // Now bookmark it and set suggest.bookmark to false.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://" + url,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "http://some-other-" + url,
        title: "test visit for http://some-other-" + url,
        providerName: PLACES_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "A bookmark",
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanup();
});

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark below autofill threshold
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkBelow_prefix_0() {
    if (!origins) {
      // See comment above suggestBookmarkFalse_visitedBookmarkBelow.
      return;
    }
    // First, make sure that `url` is below the autofill threshold.
    await PlacesTestUtils.addVisits("http://" + url);
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits("http://some-other-" + url);
    }
    let context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://some-other-" + url,
          title: "test visit for http://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://" + url,
          title: "test visit for http://" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    // Now bookmark it and set suggest.bookmark to false.
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "http://" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://some-other-" + url,
          title: "test visit for http://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://" + url,
          title: "A bookmark",
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark below autofill threshold
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: yes
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkBelow_prefix_1() {
    if (!origins) {
      // See comment above suggestBookmarkFalse_visitedBookmarkBelow.
      return;
    }
    // First, make sure that `url` is below the autofill threshold.
    await PlacesTestUtils.addVisits("ftp://" + url);
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits("ftp://some-other-" + url);
    }
    let context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://some-other-" + url,
          title: "test visit for ftp://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://" + url,
          title: "test visit for ftp://" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    // Now bookmark it and set suggest.bookmark to false.
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "ftp://" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://some-other-" + url,
          title: "test visit for ftp://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://" + url,
          title: "A bookmark",
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark below autofill threshold
//   prefix search: yes
//   prefix matches search: yes
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkBelow_prefix_2() {
    if (!origins) {
      // See comment above suggestBookmarkFalse_visitedBookmarkBelow.
      return;
    }
    // First, make sure that `url` is below the autofill threshold.
    await PlacesTestUtils.addVisits("http://non-matching-" + url);
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits("http://some-other-" + url);
    }
    let context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://some-other-" + url,
          title: "test visit for http://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://non-matching-" + url,
          title: "test visit for http://non-matching-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    // Now bookmark it and set suggest.bookmark to false.
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "http://non-matching-" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://some-other-" + url,
          title: "test visit for http://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "http://non-matching-" + url,
          title: "A bookmark",
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// Tests interaction between the suggest.history and suggest.bookmark prefs.
//
// Config:
//   suggest.history = true
//   suggest.bookmark = false
//   search for: visited bookmark below autofill threshold
//   prefix search: yes
//   prefix matches search: no
//   origin matches search: no
//
// Expected result:
//   should autofill: no
add_autofill_task(
  async function suggestBookmarkFalse_visitedBookmarkBelow_prefix_3() {
    if (!origins) {
      // See comment above suggestBookmarkFalse_visitedBookmarkBelow.
      return;
    }
    // First, make sure that `url` is below the autofill threshold.
    await PlacesTestUtils.addVisits("ftp://non-matching-" + url);
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits("ftp://some-other-" + url);
    }
    let context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://some-other-" + url,
          title: "test visit for ftp://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://non-matching-" + url,
          title: "test visit for ftp://non-matching-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    // Now bookmark it and set suggest.bookmark to false.
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "ftp://non-matching-" + url,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    context = createContext("http://" + search, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `http://${search}/`,
          fallbackTitle: `http://${search}/`,
          heuristic: true,
          iconUri: "",
          providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://some-other-" + url,
          title: "test visit for ftp://some-other-" + url,
          providerName: PLACES_PROVIDERNAME,
        }),
        makeVisitResult(context, {
          uri: "ftp://non-matching-" + url,
          title: "A bookmark",
          providerName: PLACES_PROVIDERNAME,
        }),
      ],
    });
    await cleanup();
  }
);

// When the heuristic is hidden, "ex" should autofill http://example.com/, and
// there should be an additional http://example.com/ non-autofill result.
add_autofill_task(async function hideHeuristic() {
  UrlbarPrefs.set("experimental.hideHeuristic", true);
  await PlacesTestUtils.addVisits("http://" + url);
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: "http://" + url,
    matches: [
      makeVisitResult(context, {
        uri: "http://" + url,
        title: visitTitle("http", ""),
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://" + url,
        title: "test visit for http://" + url,
      }),
    ],
  });
  await cleanup();
  UrlbarPrefs.set("experimental.hideHeuristic", false);
});
