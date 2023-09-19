/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PAGE = "https://example.com";
const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

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
  var events = Glean.shopping.surfaceReanalyzeClicked.testGetValue();

  Assert.greater(events.length, 0);
  Assert.equal(events[0].category, "shopping");
  Assert.equal(events[0].name, "surface_reanalyze_clicked");
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
  });

  Services.fog.testFlushAllChildren();
  var events = Glean.shopping.surfaceDisplayed.testGetValue();

  Assert.greater(events.length, 0);
  Assert.equal(events[0].category, "shopping");
  Assert.equal(events[0].name, "surface_displayed");
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
