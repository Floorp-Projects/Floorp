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
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.active", true]],
  });

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

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_reactivated_product_button_click() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickProductAvailableLink(browser, MOCK_STALE_PRODUCT_RESPONSE);
    }
  );

  await Services.fog.testFlushAllChildren();

  var reanalysisEvents =
    Glean.shopping.surfaceReactivatedButtonClicked.testGetValue();
  assertEventMatches(reanalysisEvents[0], {
    category: "shopping",
    name: "surface_reactivated_button_clicked",
  });
});

add_task(async function test_no_reliability_available_request_click() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickCheckReviewQualityButton(
        browser,
        MOCK_UNANALYZED_PRODUCT_RESPONSE
      );
    }
  );

  await Services.fog.testFlushAllChildren();
  var requestEvents =
    Glean.shopping.surfaceAnalyzeReviewsNoneAvailableClicked.testGetValue();

  assertEventMatches(requestEvents[0], {
    category: "shopping",
    name: "surface_analyze_reviews_none_available_clicked",
  });
});

add_task(async function test_shopping_sidebar_displayed() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.active", true]],
  });

  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    await BrowserTestUtils.waitForMutationCondition(
      shoppingButton,
      {
        attributeFilter: ["shoppingsidebaropen"],
      },
      () => shoppingButton.getAttribute("shoppingsidebaropen") == "true"
    );
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
    Assert.ok(
      BrowserTestUtils.isVisible(sidebar),
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

  await Services.fog.testFlushAllChildren();

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

  // Open a product page in a background tab, verify telemetry is not recorded.
  let backgroundTab = await BrowserTestUtils.addTab(gBrowser, PRODUCT_PAGE);
  await Services.fog.testFlushAllChildren();
  let tabSwitchEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(tabSwitchEvents, null);
  Services.fog.testResetFOG();

  // Next, switch tabs to the backgrounded product tab and verify telemetry is
  // recorded.
  await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
  await Services.fog.testFlushAllChildren();
  tabSwitchEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(1, tabSwitchEvents.length);
  assertEventMatches(tabSwitchEvents[0], {
    category: "shopping",
    name: "surface_displayed",
  });
  Services.fog.testResetFOG();

  // Finally, switch tabs again and verify telemetry is not recorded for the
  // background tab after it has been foregrounded once.
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
  await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
  await Services.fog.testFlushAllChildren();
  tabSwitchEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(tabSwitchEvents, null);
  Services.fog.testResetFOG();
  BrowserTestUtils.removeTab(backgroundTab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_shopping_card_clicks() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickShowMoreButton(browser, MOCK_ANALYZED_PRODUCT_RESPONSE);
    }
  );

  await Services.fog.testFlushAllChildren();
  var learnMoreButtonEvents =
    Glean.shopping.surfaceShowMoreReviewsButtonClicked.testGetValue();

  assertEventMatches(learnMoreButtonEvents[0], {
    category: "shopping",
    name: "surface_show_more_reviews_button_clicked",
  });
});

add_task(async function test_close_telemetry_recorded() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickCloseButton(browser, MOCK_ANALYZED_PRODUCT_RESPONSE);
    }
  );

  await Services.fog.testFlushAllChildren();

  var closeEvents = Glean.shopping.surfaceClosed.testGetValue();
  assertEventMatches(closeEvents[0], {
    category: "shopping",
    name: "surface_closed",
    extra: { source: "closeButton" },
  });

  // Ensure that the sidebar is open so we confirm the icon click closes it.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.active", true]],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function () {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    shoppingButton.click();
  });

  await Services.fog.testFlushAllChildren();
  var urlBarIconEvents = Glean.shopping.addressBarIconClicked.testGetValue();
  assertEventMatches(urlBarIconEvents[0], {
    category: "shopping",
    name: "address_bar_icon_clicked",
    extra: { action: "closed" },
  });

  var closeSurfaceEvents = Glean.shopping.surfaceClosed.testGetValue();
  assertEventMatches(closeSurfaceEvents[0], {
    category: "shopping",
    name: "surface_closed",
    extra: { source: "closeButton" },
  });

  assertEventMatches(closeSurfaceEvents[1], {
    category: "shopping",
    name: "surface_closed",
    extra: { source: "addressBarIcon" },
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_powered_by_fakespot_link() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickPoweredByFakespotLink(browser, MOCK_ANALYZED_PRODUCT_RESPONSE);
    }
  );

  await Services.fog.testFlushAllChildren();

  let fakespotLinkEvents =
    Glean.shopping.surfacePoweredByFakespotLinkClicked.testGetValue();
  assertEventMatches(fakespotLinkEvents[0], {
    category: "shopping",
    name: "surface_powered_by_fakespot_link_clicked",
  });
});

add_task(async function test_review_quality_explainer_link() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await clickReviewQualityExplainerLink(
        browser,
        MOCK_ANALYZED_PRODUCT_RESPONSE
      );
    }
  );

  await Services.fog.testFlushAllChildren();

  let qualityExplainerEvents =
    Glean.shopping.surfaceShowQualityExplainerUrlClicked.testGetValue();
  assertEventMatches(qualityExplainerEvents[0], {
    category: "shopping",
    name: "surface_show_quality_explainer_url_clicked",
  });
});

// Start with ads user enabled, then disable them, and verify telemetry.
add_task(async function test_ads_disable_button_click() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.adsEnabled", true],
      ["browser.shopping.experience2023.ads.userEnabled", true],
    ],
  });

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let mockArgs = {
        mockData: MOCK_ANALYZED_PRODUCT_RESPONSE,
        mockRecommendationData: MOCK_RECOMMENDED_ADS_RESPONSE,
      };

      await clickAdsToggle(browser, mockArgs);

      await Services.fog.testFlushAllChildren();

      // Verify the ads state was changed to disabled.
      let toggledEvents =
        Glean.shopping.surfaceAdsSettingToggled.testGetValue();
      assertEventMatches(toggledEvents[0], {
        category: "shopping",
        name: "surface_ads_setting_toggled",
        extra: { action: "disabled" },
      });

      // Verify the ads disabled state is set to true.
      Assert.equal(
        Glean.shoppingSettings.disabledAds.testGetValue(),
        true,
        "Ads should be marked as disabled"
      );
    }
  );
});

// Start with ads user disabled, then enable them, and verify telemetry.
add_task(async function test_ads_enable_button_click() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.adsEnabled", true],
      ["browser.shopping.experience2023.ads.userEnabled", false],
    ],
  });

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let mockArgs = {
        mockData: MOCK_ANALYZED_PRODUCT_RESPONSE,
        mockRecommendationData: MOCK_RECOMMENDED_ADS_RESPONSE,
      };

      await clickAdsToggle(browser, mockArgs);

      await Services.fog.testFlushAllChildren();

      // Verify the ads state was changed to enabled.
      let toggledEvents =
        Glean.shopping.surfaceAdsSettingToggled.testGetValue();
      assertEventMatches(toggledEvents[0], {
        category: "shopping",
        name: "surface_ads_setting_toggled",
        extra: { action: "enabled" },
      });

      // Verify the ads disabled state is set to false.
      Assert.equal(
        Glean.shoppingSettings.disabledAds.testGetValue(),
        false,
        "Ads should be marked as enabled"
      );
    }
  );
});

add_task(async function test_auto_open_settings_toggle() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let mockData = MOCK_ANALYZED_PRODUCT_RESPONSE;
      await clickAutoOpenToggle(browser, mockData);
      await Services.fog.testFlushAllChildren();
      let toggledEvents =
        Glean.shopping.surfaceAutoOpenSettingToggled.testGetValue();
      assertEventMatches(toggledEvents[0], {
        category: "shopping",
        name: "surface_auto_open_setting_toggled",
        extra: { action: "disabled" },
      });

      Services.fog.testResetFOG();

      // Toggle back in the other direction.
      await clickAutoOpenToggle(browser, mockData);
      await Services.fog.testFlushAllChildren();
      toggledEvents =
        Glean.shopping.surfaceAutoOpenSettingToggled.testGetValue();
      assertEventMatches(toggledEvents[0], {
        category: "shopping",
        name: "surface_auto_open_setting_toggled",
        extra: { action: "enabled" },
      });
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_no_thanks_button_click() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
      ["browser.shopping.experience2023.showKeepSidebarClosedMessage", true],
    ],
  });

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let mockArgs = {
        mockData: MOCK_ANALYZED_PRODUCT_RESPONSE,
      };

      await clickNoThanksButton(browser, mockArgs);

      await Services.fog.testFlushAllChildren();

      let noThanksButtonEvents =
        Glean.shopping.surfaceNoThanksButtonClicked.testGetValue();

      assertEventMatches(noThanksButtonEvents[0], {
        category: "shopping",
        name: "surface_no_thanks_button_clicked",
      });
    }
  );
});

add_task(async function test_auto_open_yes_keep_closed_button() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
      ["browser.shopping.experience2023.showKeepSidebarClosedMessage", true],
    ],
  });

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let mockArgs = {
        mockData: MOCK_ANALYZED_PRODUCT_RESPONSE,
      };

      await clickYesKeepClosedButton(browser, mockArgs);

      await Services.fog.testFlushAllChildren();

      let yesKeepClosedButtonEvents =
        Glean.shopping.surfaceYesKeepClosedButtonClicked.testGetValue();

      assertEventMatches(yesKeepClosedButtonEvents[0], {
        category: "shopping",
        name: "surface_yes_keep_closed_button_clicked",
      });
    }
  );
});

add_task(async function test_auto_open_user_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  Services.prefs.setBoolPref(
    "browser.shopping.experience2023.autoOpen.userEnabled",
    false
  );

  await Services.fog.testFlushAllChildren();

  Assert.equal(
    Glean.shoppingSettings.autoOpenUserDisabled.testGetValue(),
    true,
    "Auto open should be marked as disabled"
  );
});

function clickAdsToggle(browser, data) {
  return SpecialPowers.spawn(browser, [data], async args => {
    const { mockData, mockRecommendationData } = args;
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    shoppingContainer.recommendationData = Cu.cloneInto(
      mockRecommendationData,
      content
    );

    await shoppingContainer.updateComplete;

    let shoppingSettings = shoppingContainer.settingsEl;
    let toggle = shoppingSettings.recommendationsToggleEl;
    toggle.click();

    await shoppingContainer.updateComplete;
  });
}

function clickAutoOpenToggle(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);

    await shoppingContainer.updateComplete;

    let shoppingSettings = shoppingContainer.settingsEl;
    let toggle = shoppingSettings.autoOpenToggleEl;
    toggle.click();

    await shoppingContainer.updateComplete;
  });
}

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

function clickCloseButton(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let closeButton =
      shoppingContainer.shadowRoot.querySelector("#close-button");
    await closeButton.updateComplete;

    closeButton.click();
  });
}

function clickProductAvailableLink(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let shoppingMessageBar = shoppingContainer.shoppingMessageBarEl;
    await shoppingMessageBar.updateComplete;

    // calling onClickProductAvailable will fail quietly in cases where this is
    // not possible to call, so assure it exists first.
    Assert.notEqual(shoppingMessageBar, null);
    await shoppingMessageBar.onClickProductAvailable();
  });
}

function clickShowMoreButton(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let highlights = shoppingContainer.highlightsEl;
    let card = highlights.shadowRoot.querySelector("shopping-card");
    let button = card.shadowRoot.querySelector("article footer button");

    button.click();
  });
}

function clickCheckReviewQualityButton(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let button = shoppingContainer.unanalyzedProductEl.shadowRoot
      .querySelector("shopping-card")
      .querySelector("button");

    button.click();
  });
}

function clickPoweredByFakespotLink(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let settingsEl = shoppingContainer.settingsEl;
    await settingsEl.updateComplete;
    let fakespotLink = settingsEl.fakespotLearnMoreLinkEl;

    // Prevent link navigation for test.
    fakespotLink.href = undefined;
    await fakespotLink.updateComplete;

    fakespotLink.click();
  });
}

function clickReviewQualityExplainerLink(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;

    let analysisExplainerEl = shoppingContainer.analysisExplainerEl;
    await analysisExplainerEl.updateComplete;
    let reviewQualityLink = analysisExplainerEl.reviewQualityExplainerLink;

    // Prevent link navigation for test.
    reviewQualityLink.href = undefined;
    await reviewQualityLink.updateComplete;

    reviewQualityLink.click();
  });
}

function clickNoThanksButton(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    // Force the "keep closed" to appear
    shoppingContainer.showingKeepClosedMessage = true;
    await shoppingContainer.updateComplete;

    let shoppingMessageBar = shoppingContainer.keepClosedMessageBarEl;
    await shoppingMessageBar.updateComplete;

    let button = shoppingMessageBar.noThanksButtonEl;
    button.click();
  });
}

function clickYesKeepClosedButton(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    // Force the "keep closed" to appear
    shoppingContainer.showingKeepClosedMessage = true;
    await shoppingContainer.updateComplete;

    let shoppingMessageBar = shoppingContainer.keepClosedMessageBarEl;
    await shoppingMessageBar.updateComplete;

    let button = shoppingMessageBar.yesKeepClosedButtonEl;
    button.click();
  });
}
