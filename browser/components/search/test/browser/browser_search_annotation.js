/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether a visit information is annotated correctly when searching on searchbar.

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
});

const { VISIT_SOURCE_BOOKMARKED, VISIT_SOURCE_SEARCHED } = PlacesUtils.history;

async function assertDatabase({ targetURL, expected }) {
  const placesId = await PlacesTestUtils.fieldInDB(targetURL, "id");
  const db = await PlacesUtils.promiseDBConnection();
  const rows = await db.execute(
    "SELECT source, triggeringPlaceId FROM moz_historyvisits WHERE place_id = :place_id AND source = :source",
    {
      place_id: placesId,
      source: expected.source,
    }
  );
  Assert.equal(rows.length, 1);
  Assert.equal(
    rows[0].getResultByName("triggeringPlaceId"),
    null,
    `The triggeringPlaceId in database is correct for ${targetURL}`
  );
}

add_setup(async function() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await gCUITestUtils.addSearchBar();
  await SearchTestUtils.installSearchExtension({
    name: "Example",
    keyword: "@test",
  });

  const defaultEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = Services.search.getEngineByName("Example");

  registerCleanupFunction(async function() {
    Services.search.defaultEngine = defaultEngine;
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function basic() {
  const testData = [
    {
      description: "Normal search",
      input: "abc",
      resultURL: "https://example.com/?q=abc",
      expected: {
        source: VISIT_SOURCE_SEARCHED,
      },
    },
    {
      description: "Search but the url is bookmarked",
      input: "abc",
      resultURL: "https://example.com/?q=abc",
      bookmarks: [
        {
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          url: Services.io.newURI("https://example.com/?q=abc"),
          title: "test bookmark",
        },
      ],
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
      },
    },
  ];

  for (const {
    description,
    input,
    resultURL,
    bookmarks,
    expected,
  } of testData) {
    info(description);

    for (const bookmark of bookmarks || []) {
      await PlacesUtils.bookmarks.insert(bookmark);
    }

    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      resultURL
    );
    await searchInSearchbar(input);
    EventUtils.synthesizeKey("KEY_Enter");
    await onLoad;

    await assertDatabase({ targetURL: resultURL, expected });

    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
  }
});
