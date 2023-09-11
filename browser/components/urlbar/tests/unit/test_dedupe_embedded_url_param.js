/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

testEngine_setup();

add_task(async function test_embedded_url_show_up_as_places_result() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/?url=http://kitten.com/",
      title: "kitten",
    },
  ]);

  let context = createContext("kitten", {
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: "kitten",
        engineName: Services.search.defaultEngine.name,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/?url=http://kitten.com/",
        title: "kitten",
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_deduplication_of_embedded_url_autofill_result() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/?url=http://kitten.com/",
      title: "kitten",
    },
    {
      uri: "http://kitten.com/",
      title: "kitten",
    },
  ]);

  let context = createContext("kitten", {
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://kitten.com/",
        title: "kitten",
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        heuristic: true,
        providerName: "Autofill",
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_deduplication_of_embedded_url_places_result() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/?url=http://kitten.com/",
      title: "kitten",
    },
    {
      uri: "http://kitten.com/",
      title: "kitten",
    },
  ]);

  let context = createContext("kitten", {
    isPrivate: false,
    allowAutofill: false,
  });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: "kitten",
        engineName: Services.search.defaultEngine.name,
      }),
      makeVisitResult(context, {
        uri: "http://kitten.com/",
        title: "kitten",
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(
  async function test_deduplication_of_higher_frecency_embedded_url_places_result() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://example.com/?url=http://kitten.com/",
        title: "kitten",
      },
      {
        uri: "http://example.com/?url=http://kitten.com/",
        title: "kitten",
      },
      {
        uri: "http://kitten.com/",
        title: "kitten",
      },
    ]);

    let context = createContext("kitten", {
      isPrivate: false,
      allowAutofill: false,
    });

    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          heuristic: true,
          query: "kitten",
          engineName: Services.search.defaultEngine.name,
        }),
        makeVisitResult(context, {
          uri: "http://kitten.com/",
          title: "kitten",
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        }),
      ],
    });

    await cleanupPlaces();
  }
);

add_task(
  async function test_deduplication_of_embedded_encoded_url_places_result() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://example.com/?url=http%3A%2F%2Fkitten.com%2F",
        title: "kitten",
      },
      {
        uri: "http://kitten.com/",
        title: "kitten",
      },
    ]);

    let context = createContext("kitten", {
      isPrivate: false,
      allowAutofill: false,
    });

    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          heuristic: true,
          query: "kitten",
          engineName: Services.search.defaultEngine.name,
        }),
        makeVisitResult(context, {
          uri: "http://kitten.com/",
          title: "kitten",
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        }),
      ],
    });

    await cleanupPlaces();
  }
);

add_task(async function test_deduplication_of_embedded_url_switchTab_result() {
  let uri = Services.io.newURI("http://kitten.com/");

  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/?url=http://kitten.com/",
      title: "kitten",
    },
    {
      uri,
      title: "kitten",
    },
  ]);

  await addOpenPages(uri, 1);

  let context = createContext("kitten", {
    isPrivate: false,
    allowAutofill: false,
  });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: "kitten",
        engineName: Services.search.defaultEngine.name,
      }),
      makeTabSwitchResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.TAB,
        uri: "http://kitten.com/",
        title: "kitten",
      }),
    ],
  });

  await removeOpenPages(uri, 1);
  await cleanupPlaces();
});
