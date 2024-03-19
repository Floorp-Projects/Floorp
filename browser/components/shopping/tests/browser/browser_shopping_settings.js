/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the fakespot link has the expected url and utm parameters.
 */
add_task(async function test_shopping_settings_fakespot_learn_more() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_ANALYZED_PRODUCT_RESPONSE],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;

          let href = shoppingContainer.settingsEl.fakespotLearnMoreLinkEl.href;
          let url = new URL(href);
          is(url.pathname, "/our-mission");
          is(url.origin, "https://www.fakespot.com");

          let qs = url.searchParams;
          is(qs.get("utm_source"), "review-checker");
          is(qs.get("utm_campaign"), "fakespot-by-mozilla");
          is(qs.get("utm_medium"), "inproduct");
          is(qs.get("utm_term"), "core-sidebar");
        }
      );
    }
  );
});

/**
 * Tests that the ads link has the expected utm parameters.
 */
add_task(async function test_shopping_settings_ads_learn_more() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.ads.enabled", true]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_ANALYZED_PRODUCT_RESPONSE],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;

          let href = shoppingContainer.settingsEl.adsLearnMoreLinkEl.href;
          let qs = new URL(href).searchParams;

          is(qs.get("utm_campaign"), "learn-more");
          is(qs.get("utm_medium"), "inproduct");
          is(qs.get("utm_term"), "core-sidebar");
        }
      );
    }
  );
});

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.ads.enabled` is true.
 */
add_task(async function test_shopping_settings_ads_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.ads.enabled", true]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_ANALYZED_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;

          shoppingContainer.data = Cu.cloneInto(mockData, content);
          // Note (Bug 1876878): Until we have proper mocks of data passed from ShoppingSidebarChild,
          // hardcode `adsEnabled` to be passed to settings.mjs so that we can test
          // toggle for ad visibility.
          shoppingContainer.adsEnabled = true;
          await shoppingContainer.updateComplete;

          let shoppingSettings = shoppingContainer.settingsEl;
          ok(shoppingSettings, "Got the shopping-settings element");

          let adsToggle = shoppingSettings.recommendationsToggleEl;
          ok(adsToggle, "There should be an ads toggle");

          let optOutButton = shoppingSettings.optOutButtonEl;
          ok(optOutButton, "There should be an opt-out button");
        }
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.ads.enabled` is false.
 */
add_task(async function test_shopping_settings_ads_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.ads.enabled", false]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let shoppingSettings = await getSettingsDetails(
        browser,
        MOCK_POPULATED_DATA
      );
      ok(shoppingSettings.settingsEl, "Got the shopping-settings element");

      let adsToggle = shoppingSettings.recommendationsToggleEl;
      ok(!adsToggle, "There should be no ads toggle");

      let optOutButton = shoppingSettings.optOutButtonEl;
      ok(optOutButton, "There should be an opt-out button");
    }
  );

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the shopping-settings ads toggle and ad render correctly, even with
 * multiple tabs. If `browser.shopping.experience2023.ads.userEnabled`
 * is false in one tab, it should be false for all other tabs with the shopping sidebar open.
 */
add_task(async function test_settings_toggle_ad_and_multiple_tabs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.ads.enabled", true],
      ["browser.shopping.experience2023.ads.userEnabled", true],
    ],
  });

  // Tab 1 - ad is visible at first and then toggle is selected to set ads.userEnabled to false.
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:shoppingsidebar"
  );
  let browser1 = tab1.linkedBrowser;

  let mockArgs = {
    mockData: MOCK_ANALYZED_PRODUCT_RESPONSE,
    mockRecommendationData: MOCK_RECOMMENDED_ADS_RESPONSE,
  };
  await SpecialPowers.spawn(browser1, [mockArgs], async args => {
    const { mockData, mockRecommendationData } = args;
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;

    let adVisiblePromise = ContentTaskUtils.waitForCondition(() => {
      return (
        shoppingContainer.recommendedAdEl &&
        ContentTaskUtils.isVisible(shoppingContainer.recommendedAdEl)
      );
    }, "Waiting for recommended-ad to be visible");

    shoppingContainer.data = Cu.cloneInto(mockData, content);
    shoppingContainer.recommendationData = Cu.cloneInto(
      mockRecommendationData,
      content
    );
    // Note (Bug 1876878): Until we have proper mocks of data passed from ShoppingSidebarChild,
    // hardcode `adsEnabled` and `adsEnabledByUser` so that we can test ad visibility.
    shoppingContainer.adsEnabled = true;
    shoppingContainer.adsEnabledByUser = true;

    await shoppingContainer.updateComplete;
    await adVisiblePromise;

    let adEl = shoppingContainer.recommendedAdEl;
    await adEl.updateComplete;
    is(
      adEl.priceEl.textContent,
      "$" + mockRecommendationData[0].price,
      "Price is shown correctly"
    );
    is(
      adEl.linkEl.title,
      mockRecommendationData[0].name,
      "Title in link is shown correctly"
    );
    is(
      adEl.linkEl.href,
      mockRecommendationData[0].url,
      "URL for link is correct"
    );
    is(
      adEl.ratingEl.rating,
      mockRecommendationData[0].adjusted_rating,
      "MozFiveStar rating is shown correctly"
    );
    is(
      adEl.letterGradeEl.letter,
      mockRecommendationData[0].grade,
      "LetterGrade letter is shown correctly"
    );

    let shoppingSettings = shoppingContainer.settingsEl;
    ok(shoppingSettings, "Got the shopping-settings element");

    let adsToggle = shoppingSettings.recommendationsToggleEl;
    ok(adsToggle, "There should be a toggle");
    ok(adsToggle.hasAttribute("pressed"), "Toggle should have enabled state");

    ok(
      SpecialPowers.getBoolPref(
        "browser.shopping.experience2023.ads.userEnabled"
      ),
      "ads userEnabled pref should be true"
    );

    let adRemovedPromise = ContentTaskUtils.waitForCondition(() => {
      return !shoppingContainer.recommendedAdEl;
    }, "Waiting for recommended-ad to be removed");

    adsToggle.click();

    await adRemovedPromise;

    ok(!adsToggle.hasAttribute("pressed"), "Toggle should have disabled state");
    ok(
      !SpecialPowers.getBoolPref(
        "browser.shopping.experience2023.ads.userEnabled"
      ),
      "ads userEnabled pref should be false"
    );
  });

  // Tab 2 - ads.userEnabled should still be false and ad should not be visible.
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:shoppingsidebar"
  );
  let browser2 = tab2.linkedBrowser;

  await SpecialPowers.spawn(browser2, [mockArgs], async args => {
    const { mockData, mockRecommendationData } = args;
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;

    shoppingContainer.data = Cu.cloneInto(mockData, content);
    shoppingContainer.recommendationData = Cu.cloneInto(
      mockRecommendationData,
      content
    );
    // Note (Bug 1876878): Until we have proper mocks of data passed from ShoppingSidebarChild,
    // hardcode `adsEnabled` so that we can test ad visibility.
    shoppingContainer.adsEnabled = true;

    await shoppingContainer.updateComplete;

    ok(
      !shoppingContainer.recommendedAdEl,
      "There should be no ads in the new tab"
    );
    ok(
      !SpecialPowers.getBoolPref(
        "browser.shopping.experience2023.ads.userEnabled"
      ),
      "ads userEnabled pref should be false"
    );
  });

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.autoOpen.enabled` is false.
 */
add_task(async function test_shopping_settings_experiment_auto_open_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.autoOpen.enabled", false]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: PRODUCT_TEST_URL,
      gBrowser,
    },
    async browser => {
      let sidebar = gBrowser
        .getPanel(browser)
        .querySelector("shopping-sidebar");
      await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

      await SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [MOCK_ANALYZED_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;

          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let shoppingSettings = shoppingContainer.settingsEl;
          ok(shoppingSettings, "Got the shopping-settings element");
          ok(
            !shoppingSettings.wrapperEl.className.includes(
              "shopping-settings-auto-open-ui-enabled"
            ),
            "Settings card should not have a special classname with autoOpen pref disabled"
          );
          is(
            shoppingSettings.shoppingCardEl?.type,
            "accordion",
            "shopping-card type should be accordion"
          );

          /* Verify control treatment UI */
          ok(
            !shoppingSettings.autoOpenToggleEl,
            "There should be no auto-open toggle"
          );
          ok(
            !shoppingSettings.autoOpenToggleDescriptionEl,
            "There should be no description for the auto-open toggle"
          );
          ok(!shoppingSettings.dividerEl, "There should be no divider");
          ok(
            !shoppingSettings.sidebarEnabledStateEl,
            "There should be no message about the sidebar active state"
          );

          ok(
            shoppingSettings.optOutButtonEl,
            "There should be an opt-out button"
          );
        }
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.autoOpen.enabled` is true and
 * `browser.shopping.experience2023.ads.enabled is true`.
 */
add_task(
  async function test_shopping_settings_experiment_auto_open_enabled_with_ads() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.autoOpen.enabled", true],
        ["browser.shopping.experience2023.autoOpen.userEnabled", true],
        ["browser.shopping.experience2023.ads.enabled", true],
      ],
    });

    await BrowserTestUtils.withNewTab(
      {
        url: PRODUCT_TEST_URL,
        gBrowser,
      },
      async browser => {
        let sidebar = gBrowser
          .getPanel(browser)
          .querySelector("shopping-sidebar");
        await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

        await SpecialPowers.spawn(
          sidebar.querySelector("browser"),
          [MOCK_ANALYZED_PRODUCT_RESPONSE],
          async () => {
            let shoppingContainer =
              content.document.querySelector(
                "shopping-container"
              ).wrappedJSObject;

            await shoppingContainer.updateComplete;

            let shoppingSettings = shoppingContainer.settingsEl;
            ok(shoppingSettings, "Got the shopping-settings element");
            ok(
              shoppingSettings.wrapperEl.className.includes(
                "shopping-settings-auto-open-ui-enabled"
              ),
              "Settings card should have a special classname with autoOpen pref enabled"
            );
            is(
              shoppingSettings.shoppingCardEl?.type,
              "",
              "shopping-card type should be default"
            );

            ok(
              shoppingSettings.recommendationsToggleEl,
              "There should be an ads toggle"
            );

            /* Verify auto-open experiment UI */
            ok(
              shoppingSettings.autoOpenToggleEl,
              "There should be an auto-open toggle"
            );
            ok(
              shoppingSettings.autoOpenToggleDescriptionEl,
              "There should be a description for the auto-open toggle"
            );
            ok(shoppingSettings.dividerEl, "There should be a divider");
            ok(
              shoppingSettings.sidebarEnabledStateEl,
              "There should be a message about the sidebar active state"
            );

            ok(
              shoppingSettings.optOutButtonEl,
              "There should be an opt-out button"
            );
          }
        );
      }
    );

    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
  }
);

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.autoOpen.enabled` is true and
 * `browser.shopping.experience2023.ads.enabled is false`.
 */
add_task(
  async function test_shopping_settings_experiment_auto_open_enabled_no_ads() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.autoOpen.enabled", true],
        ["browser.shopping.experience2023.autoOpen.userEnabled", true],
        ["browser.shopping.experience2023.ads.enabled", false],
      ],
    });

    await BrowserTestUtils.withNewTab(
      {
        url: PRODUCT_TEST_URL,
        gBrowser,
      },
      async browser => {
        let sidebar = gBrowser
          .getPanel(browser)
          .querySelector("shopping-sidebar");
        await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

        await SpecialPowers.spawn(
          sidebar.querySelector("browser"),
          [MOCK_ANALYZED_PRODUCT_RESPONSE],
          async () => {
            let shoppingContainer =
              content.document.querySelector(
                "shopping-container"
              ).wrappedJSObject;

            await shoppingContainer.updateComplete;

            let shoppingSettings = shoppingContainer.settingsEl;
            ok(shoppingSettings, "Got the shopping-settings element");
            ok(
              shoppingSettings.wrapperEl.className.includes(
                "shopping-settings-auto-open-ui-enabled"
              ),
              "Settings card should have a special classname with autoOpen pref enabled"
            );
            is(
              shoppingSettings.shoppingCardEl?.type,
              "",
              "shopping-card type should be default"
            );

            ok(
              !shoppingSettings.recommendationsToggleEl,
              "There should be no ads toggle"
            );

            /* Verify auto-open experiment UI */
            ok(
              shoppingSettings.autoOpenToggleEl,
              "There should be an auto-open toggle"
            );
            ok(
              shoppingSettings.autoOpenToggleDescriptionEl,
              "There should be a description for the auto-open toggle"
            );
            ok(shoppingSettings.dividerEl, "There should be a divider");
            ok(
              shoppingSettings.sidebarEnabledStateEl,
              "There should be a message about the sidebar active state"
            );

            ok(
              shoppingSettings.optOutButtonEl,
              "There should be an opt-out button"
            );
          }
        );
      }
    );

    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
  }
);

/**
 * Tests that auto-open toggle state and autoOpen.userEnabled pref update correctly.
 */
add_task(async function test_settings_auto_open_toggle() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PRODUCT_TEST_URL
  );
  let browser = tab1.linkedBrowser;

  let mockArgs = {
    mockData: MOCK_ANALYZED_PRODUCT_RESPONSE,
  };

  let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
  await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

  await SpecialPowers.spawn(
    sidebar.querySelector("browser"),
    [mockArgs],
    async args => {
      const { mockData } = args;
      let shoppingContainer =
        content.document.querySelector("shopping-container").wrappedJSObject;

      shoppingContainer.data = Cu.cloneInto(mockData, content);
      await shoppingContainer.updateComplete;

      let shoppingSettings = shoppingContainer.settingsEl;
      ok(shoppingSettings, "Got the shopping-settings element");

      let autoOpenToggle = shoppingSettings.autoOpenToggleEl;
      ok(autoOpenToggle, "There should be an auto-open toggle");
      ok(
        autoOpenToggle.hasAttribute("pressed"),
        "Toggle should have enabled state"
      );

      let toggleStateChangePromise = ContentTaskUtils.waitForCondition(() => {
        return !autoOpenToggle.hasAttribute("pressed");
      }, "Waiting for auto-open toggle state to be disabled");

      autoOpenToggle.click();

      await toggleStateChangePromise;

      ok(
        !SpecialPowers.getBoolPref(
          "browser.shopping.experience2023.autoOpen.userEnabled"
        ),
        "autoOpen.userEnabled pref should be false"
      );
      ok(
        SpecialPowers.getBoolPref(
          "browser.shopping.experience2023.autoOpen.enabled"
        ),
        "autoOpen.enabled pref should still be true"
      );
      ok(
        !SpecialPowers.getBoolPref("browser.shopping.experience2023.active"),
        "Sidebar active pref should be false after pressing auto-open toggle to close the sidebar"
      );

      // Now try updating the pref directly to see if toggle will change state immediately
      await SpecialPowers.popPrefEnv();
      toggleStateChangePromise = ContentTaskUtils.waitForCondition(() => {
        return autoOpenToggle.hasAttribute("pressed");
      }, "Waiting for auto-open toggle to be enabled");

      await SpecialPowers.pushPrefEnv({
        set: [
          ["browser.shopping.experience2023.autoOpen.userEnabled", true],
          ["browser.shopping.experience2023.active", true],
        ],
      });

      await toggleStateChangePromise;
    }
  );

  BrowserTestUtils.removeTab(tab1);

  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
});
