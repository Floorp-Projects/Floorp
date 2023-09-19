/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
            !content.document.getElementById("multi-stage-message-root").hidden,
            "Survey Message container is shown"
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
