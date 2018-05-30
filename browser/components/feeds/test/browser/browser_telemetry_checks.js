/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  function getSnapShot() {
    return Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT, false);
  }
  const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
  const FEED_URI = TEST_PATH + "valid-feed.xml";

  // Ensure we don't have any pre-existing telemetry that'd mess up counts in here:
  Services.telemetry.clearScalars();
  const kScalarPrefix = "browser.feeds.";
  const kPreviewLoaded = kScalarPrefix + "preview_loaded";
  const kSubscribed = kScalarPrefix + "feed_subscribed";
  const kLivemarkCount = kScalarPrefix + "livebookmark_count";
  const kLivemarkOpened = kScalarPrefix + "livebookmark_opened";
  const kLivemarkItemOpened = kScalarPrefix + "livebookmark_item_opened";

  let scalarForContent = gMultiProcessBrowser ? "content" : "parent";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FEED_URI);
  // Ensure we get telemetry from the content process:
  let previewCount = await TestUtils.waitForCondition(() => {
    let snapshot = getSnapShot()[scalarForContent];
    return snapshot && snapshot[kPreviewLoaded];
  });
  Assert.equal(previewCount, 1, "Should register the preview in telemetry.");

  // Now check subscription. We stub out the actual code for adding the live bookmark,
  // because the dialog creates an initial copy of the bookmark when it's opened and
  // that's hard to deal with deterministically in tests.
  let old = PlacesCommandHook.addLiveBookmark;
  let createBMPromise = new Promise(resolve => {
    PlacesCommandHook.addLiveBookmark = function(...args) {
      resolve(args);
      // Return the promise because Feeds.jsm expects a promise:
      return createBMPromise;
    };
  });
  registerCleanupFunction(() => PlacesCommandHook.addLiveBookmark = old);
  await BrowserTestUtils.synthesizeMouseAtCenter("#subscribeButton", {}, tab.linkedBrowser);
  let bmArgs = await createBMPromise;
  Assert.deepEqual(bmArgs, [FEED_URI, "Example Feed"], "Should have been trying to subscribe");
  let snapshot = getSnapShot();
  Assert.equal(snapshot.parent[kSubscribed], 1, "Should have subscribed once.");

  // Now manually add a livemark in the menu and one in the bookmarks toolbar:
  let livemarks = await Promise.all([
    PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      feedURI: Services.io.newURI(FEED_URI),
    }),
    PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      feedURI: Services.io.newURI(FEED_URI),
    }),
  ]);
  registerCleanupFunction(async () => {
    for (let mark of livemarks) {
      await PlacesUtils.livemarks.removeLivemark(mark);
    }
  });

  if (document.getElementById("PersonalToolbar").getAttribute("collapsed") == "true") {
    CustomizableUI.setToolbarVisibility("PersonalToolbar", true);
    registerCleanupFunction(() => CustomizableUI.setToolbarVisibility("PersonalToolbar", false));
  }

  // Force updating telemetry:
  let {PlacesDBUtils} = ChromeUtils.import("resource://gre/modules/PlacesDBUtils.jsm", {});
  await PlacesDBUtils._telemetryForFeeds();
  Assert.equal(getSnapShot().parent[kLivemarkCount], 2,
    "Should have created two livemarks and counted them.");

  info("Waiting for livemark");
  // Check we count opening the livemark popup:
  let livemarkOnToolbar = await TestUtils.waitForCondition(
    () => document.querySelector("#PersonalToolbar .bookmark-item[livemark]"));
  let popup = livemarkOnToolbar.querySelector("menupopup");
  let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  info("Clicking on livemark");
  // Using .click() or .doCommand() doesn't seem to work.
  EventUtils.synthesizeMouseAtCenter(livemarkOnToolbar, {});
  await popupShownPromise;
  Assert.equal(getSnapShot().parent[kLivemarkOpened], 1, "Should count livemark opening");

  // And opening an item in the popup:
  let item = await TestUtils.waitForCondition(
    () => popup.querySelector("menuitem.bookmark-item"));
  item.doCommand();
  Assert.equal(getSnapShot().parent[kLivemarkItemOpened], 1, "Should count livemark item opening");
  popup.hidePopup();
  BrowserTestUtils.removeTab(tab);
});
