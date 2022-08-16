/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether a visit information is annotated correctly when clicking a tile.

if (AppConstants.platform === "macosx") {
  requestLongerTimeout(2);
}

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

const OPEN_TYPE = {
  CURRENT_BY_CLICK: 0,
  NEWTAB_BY_CLICK: 1,
  NEWTAB_BY_CONTEXTMENU: 2,
};

const FRECENCY = {
  TYPED: 2000,
  VISITED: 100,
  SPONSORED: -1,
  BOOKMARKED: 2075,
};

const {
  VISIT_SOURCE_ORGANIC,
  VISIT_SOURCE_SPONSORED,
  VISIT_SOURCE_BOOKMARKED,
} = PlacesUtils.history;

async function assertDatabase({ targetURL, expected }) {
  const frecency = await PlacesTestUtils.fieldInDB(targetURL, "frecency");
  Assert.equal(frecency, expected.frecency, "Frecency is correct");

  const placesId = await PlacesTestUtils.fieldInDB(targetURL, "id");
  const expectedTriggeringPlaceId = expected.triggerURL
    ? await PlacesTestUtils.fieldInDB(expected.triggerURL, "id")
    : null;
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
    expectedTriggeringPlaceId,
    `The triggeringPlaceId in database is correct for ${targetURL}`
  );
}

async function openAndTest({
  linkSelector,
  linkURL,
  redirectTo = null,
  openType = OPEN_TYPE.CURRENT_BY_CLICK,
  expected,
}) {
  const destinationURL = redirectTo || linkURL;

  info("Open specific link and wait for loading.");
  const isNewTab = openType !== OPEN_TYPE.CURRENT_BY_CLICK;
  const onLoad = isNewTab
    ? BrowserTestUtils.waitForNewTab(gBrowser, destinationURL, true)
    : BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        destinationURL
      );

  // If nodeIconChanged of browserPlacesViews.js is called after the target node
  // is lost during test, "No DOM node set for aPlacesNode" error occur. To avoid
  // this failure, wait for the onLocationChange event that triggers
  // nodeIconChanged to occur.
  const onLocationChanged = new Promise(resolve => {
    gBrowser.addTabsProgressListener({
      async onLocationChange(aBrowser, aWebProgress, aRequest, aLocation) {
        if (aLocation.spec === destinationURL) {
          gBrowser.removeTabsProgressListener(this);
          // Wait for an empty Promise to ensure to proceed our test after
          // finishing the processing of other onLocatoinChanged events.
          await Promise.resolve();
          resolve();
        }
      },
    });
  });

  // Wait for content is ready.
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [linkSelector, linkURL],
    async (selector, link) => {
      await ContentTaskUtils.waitForCondition(
        () => content.document.querySelector(selector).href === link
      );
    }
  );

  // Open the link by type.
  if (openType === OPEN_TYPE.NEWTAB_BY_CLICK) {
    await BrowserTestUtils.synthesizeMouseAtCenter(
      linkSelector,
      { ctrlKey: isNewTab, metaKey: isNewTab },
      gBrowser.selectedBrowser
    );
  } else if (openType === OPEN_TYPE.NEWTAB_BY_CONTEXTMENU) {
    const onPopup = BrowserTestUtils.waitForEvent(document, "popupshown");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      linkSelector,
      { type: "contextmenu" },
      gBrowser.selectedBrowser
    );
    await onPopup;
    const contextMenu = document.getElementById("contentAreaContextMenu");
    const openLinkMenuItem = contextMenu.querySelector(
      "#context-openlinkintab"
    );
    openLinkMenuItem.click();
    contextMenu.hidePopup();
  } else {
    await BrowserTestUtils.synthesizeMouseAtCenter(
      linkSelector,
      {},
      gBrowser.selectedBrowser
    );
  }

  const maybeNewTab = await onLoad;
  await onLocationChanged;

  if (isNewTab) {
    BrowserTestUtils.removeTab(maybeNewTab);
  }

  info("Check database for the destination.");
  await assertDatabase({ targetURL: destinationURL, expected });
}

async function pin(link) {
  // Setup test tile.
  NewTabUtils.pinnedLinks.pin(link, 0);
  await toggleTopsitesPref();
  await BrowserTestUtils.waitForCondition(() => {
    const sites = AboutNewTab.getTopSites();
    return (
      sites?.[0]?.url === link.url &&
      sites[0].sponsored_tile_id === link.sponsored_tile_id
    );
  }, "Waiting for top sites to be updated");
}

function unpin(link) {
  NewTabUtils.pinnedLinks.unpin(link);
}

add_setup(async function() {
  await clearHistoryAndBookmarks();
  registerCleanupFunction(async () => {
    await clearHistoryAndBookmarks();
  });
});

add_task(async function basic() {
  const testData = [
    {
      description: "Sponsored tile",
      link: {
        label: "test_label",
        url: "http://example.com/",
        sponsored_position: 1,
        sponsored_tile_id: 12345,
        sponsored_impression_url: "http://impression.example.com/",
        sponsored_click_url: "http://click.example.com/",
      },
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    },
    {
      description: "Bookmarked result",
      link: {
        label: "test_label",
        url: "http://example.com/",
      },
      bookmarks: [
        {
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          url: Services.io.newURI("http://example.com/"),
          title: "test bookmark",
        },
      ],
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description: "Sponsored and bookmarked result",
      link: {
        label: "test_label",
        url: "http://example.com/",
        sponsored_position: 1,
        sponsored_tile_id: 12345,
        sponsored_impression_url: "http://impression.example.com/",
        sponsored_click_url: "http://click.example.com/",
      },
      bookmarks: [
        {
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          url: Services.io.newURI("http://example.com/"),
          title: "test bookmark",
        },
      ],
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description: "Organic tile",
      link: {
        label: "test_label",
        url: "http://example.com/",
      },
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.TYPED,
      },
    },
  ];

  for (const { description, link, bookmarks, expected } of testData) {
    info(description);

    await BrowserTestUtils.withNewTab("about:home", async () => {
      // Setup test tile.
      await pin(link);

      // Test with new tab.
      for (const bookmark of bookmarks || []) {
        await PlacesUtils.bookmarks.insert(bookmark);
      }
      await openAndTest({
        linkSelector: ".top-site-button",
        linkURL: link.url,
        openType: OPEN_TYPE.NEWTAB_BY_CLICK,
        expected,
      });

      await clearHistoryAndBookmarks();

      // Test with same tab.
      for (const bookmark of bookmarks || []) {
        await PlacesUtils.bookmarks.insert(bookmark);
      }
      await openAndTest({
        linkSelector: ".top-site-button",
        linkURL: link.url,
        expected,
      });
      await clearHistoryAndBookmarks();

      unpin(link);
    });
  }
});

add_task(async function redirection() {
  await BrowserTestUtils.withNewTab("about:home", async () => {
    const redirectTo = "http://example.com/";
    const link = {
      label: "test_label",
      url:
        "http://example.com/browser/browser/components/newtab/test/browser/redirect_to.sjs?/",
      sponsored_position: 1,
      sponsored_tile_id: 12345,
      sponsored_impression_url: "http://impression.example.com/",
      sponsored_click_url: "http://click.example.com/",
    };

    // Setup test tile.
    await pin(link);

    // Test with new tab.
    await openAndTest({
      linkSelector: ".top-site-button",
      linkURL: link.url,
      redirectTo,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });

    // Check for URL causes the redirection.
    await assertDatabase({
      targetURL: link.url,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    });
    await clearHistoryAndBookmarks();

    // Test with same tab.
    await openAndTest({
      linkSelector: ".top-site-button",
      linkURL: link.url,
      redirectTo,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });
    // Check for URL causes the redirection.
    await assertDatabase({
      targetURL: link.url,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    });
    await clearHistoryAndBookmarks();
    unpin(link);
  });
});

add_task(async function inherit() {
  const host = "https://example.com/";
  const sameBaseDomainHost = "https://www.example.com/";
  const path = "browser/browser/components/newtab/test/browser/";
  const firstURL = `${host}${path}annotation_first.html`;
  const secondURL = `${host}${path}annotation_second.html`;
  const thirdURL = `${sameBaseDomainHost}${path}annotation_third.html`;
  const outsideURL = "https://example.org/";

  await BrowserTestUtils.withNewTab("about:home", async () => {
    const link = {
      label: "first",
      url: firstURL,
      sponsored_position: 1,
      sponsored_tile_id: 12345,
      sponsored_impression_url: "http://impression.example.com/",
      sponsored_click_url: "http://click.example.com/",
    };

    // Setup test tile.
    await pin(link);

    info("Open the tile to show first page in same tab");
    await openAndTest({
      linkSelector: ".top-site-button",
      linkURL: link.url,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    });

    info("Open link on first page to show second page in new tab");
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });
    await PlacesTestUtils.clearHistoryVisits();

    info("Open link on first page to show second page in same tab");
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });

    info(
      "Open link on second page to show third page in new tab by context menu"
    );
    await openAndTest({
      linkSelector: "a",
      linkURL: thirdURL,
      openType: OPEN_TYPE.NEWTAB_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });
    await PlacesTestUtils.clearHistoryVisits();

    info("Open link on second page to show third page in same tab");
    await openAndTest({
      linkSelector: "a",
      linkURL: thirdURL,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });

    info("Open link on third page to show outside domain page in same tab");
    await openAndTest({
      linkSelector: "a",
      linkURL: outsideURL,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.VISITED,
      },
    });

    info("Visit URL that has the same domain as sponsored link from URL bar");
    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      host
    );
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: host,
      waitForFocus: SimpleTest.waitForFocus,
    });
    EventUtils.synthesizeKey("KEY_Enter");
    await onLoad;
    await assertDatabase({
      targetURL: host,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });

    unpin(link);
    await clearHistoryAndBookmarks();
  });
});

add_task(async function timeout() {
  const base =
    "https://example.com/browser/browser/components/newtab/test/browser";
  const firstURL = `${base}/annotation_first.html`;
  const secondURL = `${base}/annotation_second.html`;

  await BrowserTestUtils.withNewTab("about:home", async () => {
    const link = {
      label: "test",
      url: firstURL,
      sponsored_position: 1,
      sponsored_tile_id: 12345,
      sponsored_impression_url: "http://impression.example.com/",
      sponsored_click_url: "http://click.example.com/",
    };

    // Setup a test tile.
    await pin(link);

    info("Open the tile");
    await openAndTest({
      linkSelector: ".top-site-button",
      linkURL: link.url,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    });

    info("Set timeout second");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.places.sponsoredSession.timeoutSecs", 1]],
    });

    info("Wait 1 sec");
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 1000));

    info("Open link on first page to show second page in new tab");
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.VISITED,
      },
    });
    await PlacesTestUtils.clearHistoryVisits();

    info("Open link on first page to show second page");
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.VISITED,
      },
    });

    unpin(link);
    await clearHistoryAndBookmarks();
  });
});
