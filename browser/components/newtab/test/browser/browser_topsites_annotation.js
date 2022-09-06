/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether a visit information is annotated correctly when clicking a tile.

if (AppConstants.platform === "macosx") {
  requestLongerTimeout(4);
} else {
  requestLongerTimeout(2);
}

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

const OPEN_TYPE = {
  CURRENT_BY_CLICK: 0,
  NEWTAB_BY_CLICK: 1,
  NEWTAB_BY_MIDDLECLICK: 2,
  NEWTAB_BY_CONTEXTMENU: 3,
  NEWWINDOW_BY_CONTEXTMENU: 4,
  NEWWINDOW_BY_CONTEXTMENU_OF_TILE: 5,
};

const FRECENCY = {
  TYPED: 2000,
  VISITED: 100,
  SPONSORED: -1,
  BOOKMARKED: 2075,
  MIDDLECLICK_TYPED: 100,
  MIDDLECLICK_BOOKMARKED: 175,
  NEWWINDOW_TYPED: 100,
  NEWWINDOW_BOOKMARKED: 175,
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

async function waitForLocationChanged(destinationURL) {
  // If nodeIconChanged of browserPlacesViews.js is called after the target node
  // is lost during test, "No DOM node set for aPlacesNode" error occur. To avoid
  // this failure, wait for the onLocationChange event that triggers
  // nodeIconChanged to occur.
  return new Promise(resolve => {
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
}

async function openAndTest({
  linkSelector,
  linkURL,
  redirectTo = null,
  openType = OPEN_TYPE.CURRENT_BY_CLICK,
  expected,
}) {
  const destinationURL = redirectTo || linkURL;

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

  info("Open specific link by type and wait for loading.");
  if (openType === OPEN_TYPE.CURRENT_BY_CLICK) {
    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      destinationURL
    );
    const onLocationChanged = waitForLocationChanged(destinationURL);

    await BrowserTestUtils.synthesizeMouseAtCenter(
      linkSelector,
      {},
      gBrowser.selectedBrowser
    );

    await onLoad;
    await onLocationChanged;
  } else if (openType === OPEN_TYPE.NEWTAB_BY_CLICK) {
    const onLoad = BrowserTestUtils.waitForNewTab(
      gBrowser,
      destinationURL,
      true
    );
    const onLocationChanged = waitForLocationChanged(destinationURL);

    await BrowserTestUtils.synthesizeMouseAtCenter(
      linkSelector,
      { ctrlKey: true, metaKey: true },
      gBrowser.selectedBrowser
    );

    const tab = await onLoad;
    await onLocationChanged;
    BrowserTestUtils.removeTab(tab);
  } else if (openType === OPEN_TYPE.NEWTAB_BY_MIDDLECLICK) {
    const onLoad = BrowserTestUtils.waitForNewTab(
      gBrowser,
      destinationURL,
      true
    );
    const onLocationChanged = waitForLocationChanged(destinationURL);

    await BrowserTestUtils.synthesizeMouseAtCenter(
      linkSelector,
      { button: 1 },
      gBrowser.selectedBrowser
    );

    const tab = await onLoad;
    await onLocationChanged;
    BrowserTestUtils.removeTab(tab);
  } else if (openType === OPEN_TYPE.NEWTAB_BY_CONTEXTMENU) {
    const onLoad = BrowserTestUtils.waitForNewTab(
      gBrowser,
      destinationURL,
      true
    );
    const onLocationChanged = waitForLocationChanged(destinationURL);

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

    const tab = await onLoad;
    await onLocationChanged;
    BrowserTestUtils.removeTab(tab);
  } else if (openType === OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU) {
    const onLoad = BrowserTestUtils.waitForNewWindow({ url: destinationURL });

    const onPopup = BrowserTestUtils.waitForEvent(document, "popupshown");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      linkSelector,
      { type: "contextmenu" },
      gBrowser.selectedBrowser
    );
    await onPopup;
    const contextMenu = document.getElementById("contentAreaContextMenu");
    const openLinkMenuItem = contextMenu.querySelector("#context-openlink");
    openLinkMenuItem.click();
    contextMenu.hidePopup();

    const win = await onLoad;
    await BrowserTestUtils.closeWindow(win);
  } else if (openType === OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU_OF_TILE) {
    const onLoad = BrowserTestUtils.waitForNewWindow({ url: destinationURL });

    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [linkSelector],
      async selector => {
        const link = content.document.querySelector(selector);
        const list = link.closest("li");
        const contextMenu = list.querySelector(".context-menu-button");
        contextMenu.click();
        const target = list.querySelector(
          "[data-l10n-id=newtab-menu-open-new-window]"
        );
        target.click();
      }
    );

    const win = await onLoad;
    await BrowserTestUtils.closeWindow(win);
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
  const SPONSORED_LINK = {
    label: "test_label",
    url: "http://example.com/",
    sponsored_position: 1,
    sponsored_tile_id: 12345,
    sponsored_impression_url: "http://impression.example.com/",
    sponsored_click_url: "http://click.example.com/",
  };
  const NORMAL_LINK = {
    label: "test_label",
    url: "http://example.com/",
  };
  const BOOKMARKS = [
    {
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: Services.io.newURI("http://example.com/"),
      title: "test bookmark",
    },
  ];

  const testData = [
    {
      description: "Sponsored tile",
      link: SPONSORED_LINK,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    },
    {
      description: "Sponsored tile in new tab by click with key",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    },
    {
      description: "Sponsored tile in new tab by middle click",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_MIDDLECLICK,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    },
    {
      description: "Sponsored tile in new tab by context menu",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    },
    {
      description: "Sponsored tile in new window by context menu",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    },
    {
      description: "Sponsored tile in new window by context menu of tile",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU_OF_TILE,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    },
    {
      description: "Bookmarked result",
      link: NORMAL_LINK,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description: "Bookmarked result in new tab by click with key",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description: "Bookmarked result in new tab by middle click",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_MIDDLECLICK,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
        frecency: FRECENCY.MIDDLECLICK_BOOKMARKED,
      },
    },
    {
      description: "Bookmarked result in new tab by context menu",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CONTEXTMENU,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
        frecency: FRECENCY.MIDDLECLICK_BOOKMARKED,
      },
    },
    {
      description: "Bookmarked result in new window by context menu",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
        frecency: FRECENCY.NEWWINDOW_BOOKMARKED,
      },
    },
    {
      description: "Bookmarked result in new window by context menu of tile",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU_OF_TILE,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_BOOKMARKED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description: "Sponsored and bookmarked result",
      link: SPONSORED_LINK,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description:
        "Sponsored and bookmarked result in new tab by click with key",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description: "Sponsored and bookmarked result in new tab by middle click",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_MIDDLECLICK,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.MIDDLECLICK_BOOKMARKED,
      },
    },
    {
      description: "Sponsored and bookmarked result in new tab by context menu",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CONTEXTMENU,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.MIDDLECLICK_BOOKMARKED,
      },
    },
    {
      description:
        "Sponsored and bookmarked result in new window by context menu",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.NEWWINDOW_BOOKMARKED,
      },
    },
    {
      description:
        "Sponsored and bookmarked result in new window by context menu of tile",
      link: SPONSORED_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU_OF_TILE,
      bookmarks: BOOKMARKS,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.BOOKMARKED,
      },
    },
    {
      description: "Organic tile",
      link: NORMAL_LINK,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.TYPED,
      },
    },
    {
      description: "Organic tile in new tab by click with key",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CLICK,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.TYPED,
      },
    },
    {
      description: "Organic tile in new tab by middle click",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_MIDDLECLICK,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.MIDDLECLICK_TYPED,
      },
    },
    {
      description: "Organic tile in new tab by context menu",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWTAB_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.MIDDLECLICK_TYPED,
      },
    },
    {
      description: "Organic tile in new window by context menu",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.NEWWINDOW_TYPED,
      },
    },
    {
      description: "Organic tile in new window by context menu of tile",
      link: NORMAL_LINK,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU_OF_TILE,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.TYPED,
      },
    },
  ];

  for (const { description, link, openType, bookmarks, expected } of testData) {
    info(description);

    await BrowserTestUtils.withNewTab("about:home", async () => {
      // Setup test tile.
      await pin(link);

      for (const bookmark of bookmarks || []) {
        await PlacesUtils.bookmarks.insert(bookmark);
      }

      await openAndTest({
        linkSelector: ".top-site-button",
        linkURL: link.url,
        openType,
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

    info("Open link on first page to show second page in new window");
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });
    await PlacesTestUtils.clearHistoryVisits();

    info(
      "Open link on first page to show second page in new tab by click with key"
    );
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

    info(
      "Open link on first page to show second page in new tab by middle click"
    );
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      openType: OPEN_TYPE.NEWTAB_BY_MIDDLECLICK,
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

    info("Open link on first page to show second page in new window");
    await openAndTest({
      linkSelector: "a",
      linkURL: thirdURL,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
        triggerURL: link.url,
      },
    });
    await PlacesTestUtils.clearHistoryVisits();

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

    info(
      "Open link on second page to show third page in new tab by middle click"
    );
    await openAndTest({
      linkSelector: "a",
      linkURL: thirdURL,
      openType: OPEN_TYPE.NEWTAB_BY_MIDDLECLICK,
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

    info("Open link on first page to show second page in new window");
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      openType: OPEN_TYPE.NEWWINDOW_BY_CONTEXTMENU,
      expected: {
        source: VISIT_SOURCE_ORGANIC,
        frecency: FRECENCY.VISITED,
      },
    });
    await PlacesTestUtils.clearHistoryVisits();

    info(
      "Open link on first page to show second page in new tab by click with key"
    );
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

    info(
      "Open link on first page to show second page in new tab by middle click"
    );
    await openAndTest({
      linkSelector: "a",
      linkURL: secondURL,
      openType: OPEN_TYPE.NEWTAB_BY_MIDDLECLICK,
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

add_task(async function fixup() {
  await BrowserTestUtils.withNewTab("about:home", async () => {
    const destinationURL = "http://example.com/?a";
    const link = {
      label: "test",
      url: "http://example.com?a",
      sponsored_position: 1,
      sponsored_tile_id: 12345,
      sponsored_impression_url: "http://impression.example.com/",
      sponsored_click_url: "http://click.example.com/",
    };

    info("Setup pin");
    await pin(link);

    info("Click sponsored tile");
    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      destinationURL
    );
    const onLocationChanged = waitForLocationChanged(destinationURL);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      ".top-site-button",
      {},
      gBrowser.selectedBrowser
    );
    await onLoad;
    await onLocationChanged;

    info("Check the DB");
    await assertDatabase({
      targetURL: destinationURL,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    });

    info("Clean up");
    unpin(link);
    await clearHistoryAndBookmarks();
  });
});

add_task(async function noTriggeringURL() {
  await BrowserTestUtils.withNewTab("about:home", async browser => {
    Services.telemetry.clearScalars();

    const dummyTriggeringSponsoredURL =
      "http://example.com/dummyTriggeringSponsoredURL";
    const targetURL = "http://example.com/";

    info("Setup dummy triggering sponsored URL");
    browser.setAttribute("triggeringSponsoredURL", dummyTriggeringSponsoredURL);
    browser.setAttribute("triggeringSponsoredURLVisitTimeMS", Date.now());

    info("Open URL whose host is the same as dummy triggering sponsored URL");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: targetURL,
      waitForFocus: SimpleTest.waitForFocus,
    });
    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      targetURL
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await onLoad;

    info("Check DB");
    await assertDatabase({
      targetURL,
      expected: {
        source: VISIT_SOURCE_SPONSORED,
        frecency: FRECENCY.SPONSORED,
      },
    });

    info("Check telemetry");
    const scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
    TelemetryTestUtils.assertScalar(
      scalars,
      "places.sponsored_visit_no_triggering_url",
      1
    );

    await clearHistoryAndBookmarks();
  });
});
