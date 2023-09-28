/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const currentTime = Date.now() / 1000;
const time25HrsAgo = currentTime - 25 * 60 * 60;
const time1HrAgo = currentTime - 1 * 60 * 60;

add_task(async function test_setup() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let childActor = content.windowGlobalChild.getExistingActor(
          "AboutWelcomeShopping"
        );
        childActor.resetChildStates();
      });
    }
  );
});

/**
 * Test to check survey renders when show survey conditions are met
 */
add_task(async function test_showSurvey_Enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.survey.enabled", true],
      ["browser.shopping.experience2023.survey.hasSeen", false],
      ["browser.shopping.experience2023.survey.pdpVisits", 5],
      ["browser.shopping.experience2023.survey.optedInTime", time25HrsAgo],
    ],
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
          const { TestUtils } = ChromeUtils.importESModule(
            "resource://testing-common/TestUtils.sys.mjs"
          );
          let surveyPrefChanged = TestUtils.waitForPrefChange(
            "browser.shopping.experience2023.survey.hasSeen"
          );
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);

          // Manually send data update event, as it isn't set due to the lack of mock APIs.
          // TODO: Support for the mocks will be added in Bug 1853474.
          let mockObj = {
            data: mockData,
            productUrl: "https://example.com/product/1234",
          };
          let evt = new content.CustomEvent("Update", {
            bubbles: true,
            detail: Cu.cloneInto(mockObj, content),
          });
          content.document.dispatchEvent(evt);

          await shoppingContainer.updateComplete;
          await surveyPrefChanged;

          let childActor = content.windowGlobalChild.getExistingActor(
            "AboutWelcomeShopping"
          );

          ok(childActor.surveyEnabled, "Survey is Enabled");

          let surveyScreen = await ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector(
                "shopping-container .screen.SHOPPING_MICROSURVEY_SCREEN_1"
              ),
            "survey-screen"
          );

          ok(surveyScreen, "Survey screen is rendered");

          ok(
            childActor.showMicroSurvey,
            "Show Survey targeting conditions met"
          );
          ok(
            content.document
              .getElementById("steps")
              .getAttribute("data-l10n-id") ===
              "shopping-onboarding-welcome-steps-indicator-label",
            "Steps indicator has appropriate fluent ID"
          );
          ok(
            !content.document.getElementById("multi-stage-message-root").hidden,
            "Survey Message container is shown"
          );
          ok(
            content.document.querySelector(".dismiss-button"),
            "Dismiss button is shown"
          );

          let survey_seen_status = Services.prefs.getBoolPref(
            "browser.shopping.experience2023.survey.hasSeen",
            false
          );
          ok(survey_seen_status, "Survey pref state is updated");
          childActor.resetChildStates();
        }
      );
    }
  );
  await SpecialPowers.popPrefEnv();
});

/**
 * Test to check survey is hidden when survey enabled pref is false
 */
add_task(async function test_showSurvey_Disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.survey.enabled", false],
      ["browser.shopping.experience2023.survey.hasSeen", false],
      ["browser.shopping.experience2023.survey.pdpVisits", 5],
      ["browser.shopping.experience2023.survey.optedInTime", time25HrsAgo],
    ],
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

          // Manually send data update event, as it isn't set due to the lack of mock APIs.
          // TODO: Support for the mocks will be added in Bug 1853474.
          let mockObj = {
            data: mockData,
            productUrl: "https://example.com/product/1234",
          };
          let evt = new content.CustomEvent("Update", {
            bubbles: true,
            detail: Cu.cloneInto(mockObj, content),
          });
          content.document.dispatchEvent(evt);

          await shoppingContainer.updateComplete;

          let childActor = content.windowGlobalChild.getExistingActor(
            "AboutWelcomeShopping"
          );

          ok(!childActor.surveyEnabled, "Survey is disabled");

          let surveyScreen = content.document.querySelector(
            "shopping-container .screen.SHOPPING_MICROSURVEY_SCREEN_1"
          );

          ok(!surveyScreen, "Survey screen is not rendered");
          ok(
            !childActor.showMicroSurvey,
            "Show Survey targeting conditions are not met"
          );
          ok(
            content.document.getElementById("multi-stage-message-root").hidden,
            "Survey Message container is hidden"
          );

          childActor.resetChildStates();
        }
      );
    }
  );
  await SpecialPowers.popPrefEnv();
});

/**
 * Test to check survey display logic respects 24 hours after Opt-in rule
 */
add_task(async function test_24_hr_since_optin_rule() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.survey.enabled", true],
      ["browser.shopping.experience2023.survey.hasSeen", false],
      ["browser.shopping.experience2023.survey.pdpVisits", 5],
      ["browser.shopping.experience2023.survey.optedInTime", time1HrAgo],
    ],
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
          await shoppingContainer.updateComplete;

          let childActor = content.windowGlobalChild.getExistingActor(
            "AboutWelcomeShopping"
          );

          let surveyScreen = content.document.querySelector(
            "shopping-container .screen.SHOPPING_MICROSURVEY_SCREEN_1"
          );

          ok(!surveyScreen, "Survey screen is not rendered");
          ok(
            !childActor.showMicroSurvey,
            "Show Survey 24 hours after opt in conditions are not met"
          );
          ok(
            content.document.getElementById("multi-stage-message-root").hidden,
            "Survey Message container is hidden"
          );

          childActor.resetChildStates();
        }
      );
    }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_confirmation_screen() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.survey.enabled", true],
      ["browser.shopping.experience2023.survey.hasSeen", false],
      ["browser.shopping.experience2023.survey.pdpVisits", 5],
      ["browser.shopping.experience2023.survey.optedInTime", time25HrsAgo],
    ],
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
          async function clickVisibleElement(selector) {
            await ContentTaskUtils.waitForCondition(
              () => content.document.querySelector(selector),
              `waiting for selector ${selector}`,
              200, // interval
              100 // maxTries
            );
            content.document.querySelector(selector).click();
          }

          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);

          // Manually send data update event, as it isn't set due to the lack of mock APIs.
          // TODO: Support for the mocks will be added in Bug 1853474.
          let mockObj = {
            data: mockData,
            productUrl: "https://example.com/product/1234",
          };
          let evt = new content.CustomEvent("Update", {
            bubbles: true,
            detail: Cu.cloneInto(mockObj, content),
          });
          content.document.dispatchEvent(evt);

          await shoppingContainer.updateComplete;

          let surveyScreen1 = await ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector(
                "shopping-container .screen.SHOPPING_MICROSURVEY_SCREEN_1"
              ),
            "survey-screen"
          );

          ok(surveyScreen1, "Survey screen 1 is rendered");
          clickVisibleElement("#radio-1");
          clickVisibleElement("button.primary");

          let surveyScreen2 = await ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector(
                "shopping-container .screen.SHOPPING_MICROSURVEY_SCREEN_2"
              ),
            "survey-screen"
          );
          ok(surveyScreen2, "Survey screen 2 is rendered");
          clickVisibleElement("#radio-1");
          clickVisibleElement("button.primary");

          let confirmationScreen = await ContentTaskUtils.waitForCondition(
            () => content.document.querySelector("shopping-message-bar"),
            "survey-screen"
          );

          ok(confirmationScreen, "Survey confirmation screen is rendered");
        }
      );
    }
  );
  await SpecialPowers.popPrefEnv();
});
