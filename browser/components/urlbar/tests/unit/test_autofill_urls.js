/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const HEURISTIC_FALLBACK_PROVIDERNAME = "HeuristicFallback";
const PLACES_PROVIDERNAME = "Places";

// "example.com/foo/" should match http://example.com/foo/.
testEngine_setup();

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
});
Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

add_task(async function multipleSlashes() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo/",
    },
  ]);
  let context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "http://example.com/foo/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com/foo/",
        title: "test visit for http://example.com/foo/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// "example.com:8888/f" should match http://example.com:8888/foo.
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo",
    },
  ]);
  let context = createContext("example.com:8888/f", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/foo",
    completed: "http://example.com:8888/foo",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo",
        title: "test visit for http://example.com:8888/foo",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// "example.com:8999/f" should *not* autofill http://example.com:8888/foo.
add_task(async function portNoMatch() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo",
    },
  ]);
  let context = createContext("example.com:8999/f", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://example.com:8999/f",
        title: "http://example.com:8999/f",
        iconUri: "page-icon:http://example.com:8999/",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill to the next slash
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo/bar/baz",
    },
  ]);
  let context = createContext("example.com:8888/foo/b", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/foo/bar/",
    completed: "http://example.com:8888/foo/bar/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/",
        title: "example.com:8888/foo/bar/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/baz",
        title: "test visit for http://example.com:8888/foo/bar/baz",
        tags: [],
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill to the next slash, end of url
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo/bar/baz",
    },
  ]);
  let context = createContext("example.com:8888/foo/bar/b", {
    isPrivate: false,
  });
  await check_results({
    context,
    autofilled: "example.com:8888/foo/bar/baz",
    completed: "http://example.com:8888/foo/bar/baz",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/baz",
        title: "test visit for http://example.com:8888/foo/bar/baz",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill with case insensitive from history and bookmark.
add_task(async function caseInsensitiveFromHistoryAndBookmark() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);

  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo",
    },
  ]);

  await testCaseInsensitive();

  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  await cleanupPlaces();
});

// autofill with case insensitive from history.
add_task(async function caseInsensitiveFromHistory() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);

  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo",
    },
  ]);

  await testCaseInsensitive();

  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  await cleanupPlaces();
});

// autofill with case insensitive from bookmark.
add_task(async function caseInsensitiveFromBookmark() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);

  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://example.com/foo",
  });

  await testCaseInsensitive(true);

  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  await cleanupPlaces();
});

// should *not* autofill if the URI fragment does not match with case-sensitive.
add_task(async function uriFragmentCaseSensitive() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/#TEST",
    },
  ]);
  const context = createContext("http://example.com/#t", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://example.com/#t",
        title: "http://example.com/#t",
        heuristic: true,
      }),
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://example.com/#TEST",
        title: "test visit for http://example.com/#TEST",
        tags: [],
      }),
    ],
  });

  await cleanupPlaces();
});

// should autofill if the URI fragment matches with case-sensitive.
add_task(async function uriFragmentCaseSensitive() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/#TEST",
    },
  ]);
  const context = createContext("http://example.com/#T", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://example.com/#TEST",
    completed: "http://example.com/#TEST",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://example.com/#TEST",
        title: "test visit for http://example.com/#TEST",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function uriCase() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/ABC/DEF",
    },
  ]);

  const testData = [
    {
      input: "example.COM",
      expected: {
        autofilled: "example.COM/",
        completed: "http://example.com/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/",
              title: "example.com",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "example.COM/",
      expected: {
        autofilled: "example.COM/",
        completed: "http://example.com/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/",
              title: "example.com/",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "example.COM/a",
      expected: {
        autofilled: "example.COM/aBC/",
        completed: "http://example.com/ABC/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/",
              title: "example.com/ABC/",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "example.com/ab",
      expected: {
        autofilled: "example.com/abC/",
        completed: "http://example.com/ABC/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/",
              title: "example.com/ABC/",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "example.com/abc",
      expected: {
        autofilled: "example.com/abc/",
        completed: "http://example.com/ABC/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/",
              title: "example.com/ABC/",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "example.com/abc/",
      expected: {
        autofilled: "example.com/abc/",
        completed: "http://example.com/abc/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/abc/",
              title: "example.com/abc/",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "example.com/abc/d",
      expected: {
        autofilled: "example.com/abc/dEF",
        completed: "http://example.com/ABC/DEF",
        hasAutofillTitle: true,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
              heuristic: true,
            }),
        ],
      },
    },
    {
      input: "example.com/abc/de",
      expected: {
        autofilled: "example.com/abc/deF",
        completed: "http://example.com/ABC/DEF",
        hasAutofillTitle: true,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
              heuristic: true,
            }),
        ],
      },
    },
    {
      input: "example.com/abc/def",
      expected: {
        autofilled: "example.com/abc/def",
        completed: "http://example.com/abc/def",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/abc/def",
              title: "example.com/abc/def",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "http://example.com/a",
      expected: {
        autofilled: "http://example.com/aBC/",
        completed: "http://example.com/ABC/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/",
              title: "example.com/ABC/",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "http://example.com/abc/",
      expected: {
        autofilled: "http://example.com/abc/",
        completed: "http://example.com/abc/",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/abc/",
              title: "example.com/abc/",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "http://example.com/abc/d",
      expected: {
        autofilled: "http://example.com/abc/dEF",
        completed: "http://example.com/ABC/DEF",
        hasAutofillTitle: true,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
              heuristic: true,
            }),
        ],
      },
    },
    {
      input: "http://example.com/abc/def",
      expected: {
        autofilled: "http://example.com/abc/def",
        completed: "http://example.com/abc/def",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/abc/def",
              title: "example.com/abc/def",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
    {
      input: "http://eXAMple.com/ABC/DEF",
      expected: {
        autofilled: "http://eXAMple.com/ABC/DEF",
        completed: "http://example.com/ABC/DEF",
        hasAutofillTitle: true,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
              heuristic: true,
            }),
        ],
      },
    },
    {
      input: "http://eXAMple.com/abc/def",
      expected: {
        autofilled: "http://eXAMple.com/abc/def",
        completed: "http://example.com/abc/def",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/abc/def",
              title: "example.com/abc/def",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "http://example.com/ABC/DEF",
              title: "test visit for http://example.com/ABC/DEF",
            }),
        ],
      },
    },
  ];

  for (const { input, expected } of testData) {
    const context = createContext(input, {
      isPrivate: false,
    });
    await check_results({
      context,
      autofilled: expected.autofilled,
      completed: expected.completed,
      hasAutofillTitle: expected.hasAutofillTitle,
      matches: expected.results.map(f => f(context)),
    });
  }

  await cleanupPlaces();
});

async function testCaseInsensitive(isBookmark = false) {
  const testData = [
    {
      input: "example.com/F",
      expectedAutofill: "example.com/Foo",
    },
    {
      // Test with prefix.
      input: "http://example.com/F",
      expectedAutofill: "http://example.com/Foo",
    },
  ];

  for (const { input, expectedAutofill } of testData) {
    const context = createContext(input, {
      isPrivate: false,
    });
    await check_results({
      context,
      autofilled: expectedAutofill,
      completed: "http://example.com/foo",
      hasAutofillTitle: true,
      matches: [
        makeVisitResult(context, {
          uri: "http://example.com/foo",
          title: isBookmark
            ? "A bookmark"
            : "test visit for http://example.com/foo",
          heuristic: true,
        }),
      ],
    });
  }

  await cleanupPlaces();
}

// Checks a URL with an origin that looks like a prefix: a scheme with no dots +
// a port.
add_task(async function originLooksLikePrefix1() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://localhost:8888/foo",
    },
  ]);
  const context = createContext("localhost:8888/f", { isPrivate: false });
  await check_results({
    context,
    autofilled: "localhost:8888/foo",
    completed: "http://localhost:8888/foo",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo",
        title: "test visit for http://localhost:8888/foo",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// Same as previous (originLooksLikePrefix1) but uses a URL whose path has two
// slashes, not one.
add_task(async function originLooksLikePrefix2() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://localhost:8888/foo/bar",
    },
  ]);

  let context = createContext("localhost:8888/f", { isPrivate: false });
  await check_results({
    context,
    autofilled: "localhost:8888/foo/",
    completed: "http://localhost:8888/foo/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo/",
        title: "localhost:8888/foo/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo/bar",
        title: "test visit for http://localhost:8888/foo/bar",
        providerName: PLACES_PROVIDERNAME,
        tags: [],
      }),
    ],
  });

  context = createContext("localhost:8888/foo/b", { isPrivate: false });
  await check_results({
    context,
    autofilled: "localhost:8888/foo/bar",
    completed: "http://localhost:8888/foo/bar",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo/bar",
        title: "test visit for http://localhost:8888/foo/bar",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// Checks view-source pages as a prefix
// Uses bookmark because addVisits does not allow non-http uri's
add_task(async function viewSourceAsPrefix() {
  let address = "view-source:https://www.example.com/";
  let title = "A view source bookmark";
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: address,
    title,
  });

  let testData = [
    {
      input: "view-source:h",
      completed: "view-source:https:/",
      autofilled: "view-source:https:/",
    },
    {
      input: "view-source:http",
      completed: "view-source:https:/",
      autofilled: "view-source:https:/",
    },
    {
      input: "VIEW-SOURCE:http",
      completed: "view-source:https:/",
      autofilled: "VIEW-SOURCE:https:/",
    },
  ];

  // Only autofills from view-source:h to view-source:https:/
  for (let { input, completed, autofilled } of testData) {
    let context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      completed,
      autofilled,
      hasAutofillTitle: false,
      matches: [
        {
          heuristic: true,
          type: UrlbarUtils.RESULT_TYPE.URL,
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        },
        makeBookmarkResult(context, {
          uri: address,
          iconUri: "chrome://global/skin/icons/defaultFavicon.svg",
          title,
        }),
      ],
    });
  }

  await cleanupPlaces();
});

// Checks data url prefixes
// Uses bookmark because addVisits does not allow non-http uri's
add_task(async function dataAsPrefix() {
  let address = "data:text/html,%3Ch1%3EHello%2C World!%3C%2Fh1%3E";
  let title = "A data url bookmark";
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: address,
    title,
  });

  let testData = [
    {
      input: "data:t",
      completed: "data:text/",
      autofilled: "data:text/",
    },
    {
      input: "data:text",
      completed: "data:text/",
      autofilled: "data:text/",
    },
    {
      input: "DATA:text",
      completed: "data:text/",
      autofilled: "DATA:text/",
    },
  ];

  for (let { input, completed, autofilled } of testData) {
    let context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      completed,
      autofilled,
      hasAutofillTitle: false,
      matches: [
        {
          heuristic: true,
          type: UrlbarUtils.RESULT_TYPE.URL,
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        },
        makeBookmarkResult(context, {
          uri: address,
          iconUri: "chrome://global/skin/icons/defaultFavicon.svg",
          title,
        }),
      ],
    });
  }

  await cleanupPlaces();
});

// Checks about prefixes
add_task(async function aboutAsPrefix() {
  let testData = [
    {
      input: "about:abou",
      completed: "about:about",
      autofilled: "about:about",
    },
    {
      input: "ABOUT:abou",
      completed: "about:about",
      autofilled: "ABOUT:about",
    },
  ];

  for (let { input, completed, autofilled } of testData) {
    let context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      completed,
      autofilled,
      matches: [
        {
          heuristic: true,
          type: UrlbarUtils.RESULT_TYPE.URL,
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        },
      ],
    });
  }

  await cleanupPlaces();
});

// Checks a URL that has www name in history.
add_task(async function wwwHistory() {
  const testData = [
    {
      input: "example.com/",
      visitHistory: [{ uri: "http://www.example.com/", title: "Example" }],
      expected: {
        autofilled: "example.com/",
        completed: "http://www.example.com/",
        hasAutofillTitle: true,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "http://www.example.com/",
              title: "Example",
              heuristic: true,
            }),
        ],
      },
    },
    {
      input: "https://example.com/",
      visitHistory: [{ uri: "https://www.example.com/", title: "Example" }],
      expected: {
        autofilled: "https://example.com/",
        completed: "https://www.example.com/",
        hasAutofillTitle: true,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "https://www.example.com/",
              title: "Example",
              heuristic: true,
            }),
        ],
      },
    },
    {
      input: "https://example.com/abc",
      visitHistory: [{ uri: "https://www.example.com/abc", title: "Example" }],
      expected: {
        autofilled: "https://example.com/abc",
        completed: "https://www.example.com/abc",
        hasAutofillTitle: true,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "https://www.example.com/abc",
              title: "Example",
              heuristic: true,
            }),
        ],
      },
    },
    {
      input: "https://example.com/ABC",
      visitHistory: [{ uri: "https://www.example.com/abc", title: "Example" }],
      expected: {
        autofilled: "https://example.com/ABC",
        completed: "https://www.example.com/ABC",
        hasAutofillTitle: false,
        results: [
          context =>
            makeVisitResult(context, {
              uri: "https://www.example.com/ABC",
              title: "https://www.example.com/ABC",
              heuristic: true,
            }),
          context =>
            makeVisitResult(context, {
              uri: "https://www.example.com/abc",
              title: "Example",
            }),
        ],
      },
    },
  ];

  for (const { input, visitHistory, expected } of testData) {
    await PlacesTestUtils.addVisits(visitHistory);
    const context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      completed: expected.completed,
      autofilled: expected.autofilled,
      hasAutofillTitle: expected.hasAutofillTitle,
      matches: expected.results.map(f => f(context)),
    });
    await cleanupPlaces();
  }
});
