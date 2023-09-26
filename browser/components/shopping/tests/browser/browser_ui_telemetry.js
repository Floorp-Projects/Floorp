/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PAGE = "https://example.com";
const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

function assertEventMatches(gleanEvent, requiredValues) {
  let limitedEvent = Object.assign({}, gleanEvent);
  for (let k of Object.keys(limitedEvent)) {
    if (!requiredValues.hasOwnProperty(k)) {
      delete limitedEvent[k];
    }
  }
  return Assert.deepEqual(limitedEvent, requiredValues);
}

add_task(async function test_shopping_reanalysis_event() {
  // testFlushAllChildren() is necessary to deal with the event being
  // recorded in content, but calling testGetValue() in parent.
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickReAnalyzeLink(browser, MOCK_STALE_PRODUCT_RESPONSE);
    }
  );

  await Services.fog.testFlushAllChildren();
  var staleAnalysisEvents =
    Glean.shopping.surfaceStaleAnalysisShown.testGetValue();

  assertEventMatches(staleAnalysisEvents[0], {
    category: "shopping",
    name: "surface_stale_analysis_shown",
  });

  var reanalysisRequestedEvents =
    Glean.shopping.surfaceReanalyzeClicked.testGetValue();

  assertEventMatches(reanalysisRequestedEvents[0], {
    category: "shopping",
    name: "surface_reanalyze_clicked",
  });
});

add_task(async function test_shopping_UI_chevron_clicks() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickSettingsChevronButton(browser, MOCK_ANALYZED_PRODUCT_RESPONSE);
    }
  );

  await Services.fog.testFlushAllChildren();
  var events = Glean.shopping.surfaceSettingsExpandClicked.testGetValue();

  Assert.greater(events.length, 0);
  Assert.equal(events[0].category, "shopping");
  Assert.equal(events[0].name, "surface_settings_expand_clicked");
});

add_task(async function test_shopping_sidebar_displayed() {
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
    await sidebar.complete;

    Assert.ok(
      BrowserTestUtils.is_visible(sidebar),
      "Sidebar should be visible."
    );

    // open a new tab onto a page where sidebar is not visible.
    let contentTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: CONTENT_PAGE,
    });

    // change the focused tab a few times to ensure we don't increment on tab
    // switch.
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
    await BrowserTestUtils.switchTab(gBrowser, contentTab);
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);

    BrowserTestUtils.removeTab(contentTab);
  });

  Services.fog.testFlushAllChildren();

  var displayedEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(1, displayedEvents.length);
  assertEventMatches(displayedEvents[0], {
    category: "shopping",
    name: "surface_displayed",
  });

  var addressBarIconDisplayedEvents =
    Glean.shopping.addressBarIconDisplayed.testGetValue();
  assertEventMatches(addressBarIconDisplayedEvents[0], {
    category: "shopping",
    name: "address_bar_icon_displayed",
  });

  // reset FOG and check a page that should NOT have these events
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");

    Assert.equal(sidebar, null);
  });

  var emptyDisplayedEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  var emptyAddressBarIconDisplayedEvents =
    Glean.shopping.addressBarIconDisplayed.testGetValue();

  Assert.equal(emptyDisplayedEvents, null);
  Assert.equal(emptyAddressBarIconDisplayedEvents, null);
});

function clickReAnalyzeLink(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let shoppingMessageBar = shoppingContainer.shoppingMessageBarEl;
    await shoppingMessageBar.updateComplete;

    await shoppingMessageBar.onClickAnalysisButton();

    return "clicked";
  });
}

function clickSettingsChevronButton(browser, data) {
  // waitForCondition relies on setTimeout, can cause intermittent test
  // failures. Unfortunately, there is not yet a DOM Mutation observer
  // equivalent in a utility that's available in the content process.

  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);

    await shoppingContainer.updateComplete;
    let shoppingSettings = shoppingContainer.settingsEl;
    await shoppingSettings.updateComplete;
    let shoppingCard =
      shoppingSettings.shadowRoot.querySelector("shopping-card");
    await shoppingCard.updateComplete;

    let detailsEl = shoppingCard.detailsEl;
    await detailsEl.updateComplete;

    await ContentTaskUtils.waitForCondition(() =>
      detailsEl.querySelector(".chevron-icon")
    );

    let chevron = detailsEl.querySelector(".chevron-icon");

    chevron.click();
    return "clicked";
  });
}
