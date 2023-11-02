/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests history and bookmark results show up when search service
 * initialization has failed.
 */

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

const searchService = Services.search.wrappedJSObject;

add_setup(async function setup() {
  searchService.errorToThrowInTest = "Settings";

  // When search service fails, we want the promise rejection to be uncaught
  // so we can continue running the test.
  PromiseTestUtils.expectUncaughtRejection(
    /Fake Settings error during search service initialization./
  );

  registerCleanupFunction(async () => {
    searchService.errorToThrowInTest = null;
    await cleanupPlaces();
  });
});

add_task(
  async function test_bookmark_results_are_shown_when_search_service_failed() {
    Assert.equal(
      searchService.isInitialized,
      false,
      "Search Service should not be initialized."
    );

    info("Add a bookmark");
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "http://cat.com/",
      title: "cat",
    });

    let context = createContext("cat", {
      isPrivate: false,
      allowAutofill: false,
    });

    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          uri: "http://cat/",
          heuristic: true,
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          fallbackTitle: "http://cat/",
        }),
        makeBookmarkResult(context, {
          title: "cat",
          uri: "http://cat.com/",
          source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
        }),
      ],
    });

    Assert.equal(
      searchService.isInitialized,
      true,
      "Search Service should have finished its attempt to initialize."
    );

    Assert.equal(
      searchService.hasSuccessfullyInitialized,
      false,
      "Search Service should have failed to initialize."
    );
    await cleanupPlaces();
  }
);

add_task(
  async function test_history_results_are_shown_when_search_service_failed() {
    Assert.equal(
      searchService.isInitialized,
      true,
      "Search Service should have finished its attempt to initialize in the previous test."
    );

    Assert.equal(
      searchService.hasSuccessfullyInitialized,
      false,
      "Search Service should have failed to initialize."
    );

    info("visit a url in history");
    await PlacesTestUtils.addVisits({
      uri: "http://example.com/",
      title: "example",
    });

    let context = createContext("example", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          type: 3,
          title: "example",
          uri: "http://example.com/",
          heuristic: true,
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        }),
      ],
    });
  }
);
