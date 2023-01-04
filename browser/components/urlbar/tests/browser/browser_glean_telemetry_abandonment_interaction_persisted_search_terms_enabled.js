/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test abandonment telemetry with persisted search terms enabled.

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.showSearchTerms.featureGate", true],
      ["browser.urlbar.showSearchTerms.enabled", true],
      ["browser.search.widget.inNavBar", false],
    ],
  });
});

add_task(async function interaction_persisted_search_terms() {
  await doTest(async browser => {
    await openPopup("x");
    await doEnter();

    await openPopup("x");
    await doBlur();

    assertAbandonmentTelemetry([{ interaction: "persisted_search_terms" }]);
  });
});

add_task(async function interaction_persisted_search_terms_restarted_refined() {
  const testData = [
    {
      firstInput: "x",
      // Just move the focus to the URL bar after engagement.
      secondInput: null,
      expected: "persisted_search_terms",
    },
    {
      firstInput: "x",
      secondInput: "x",
      expected: "persisted_search_terms",
    },
    {
      firstInput: "x",
      secondInput: "y",
      expected: "persisted_search_terms_restarted",
    },
    {
      firstInput: "x",
      secondInput: "x y",
      expected: "persisted_search_terms_refined",
    },
    {
      firstInput: "x y",
      secondInput: "x",
      expected: "persisted_search_terms_refined",
    },
  ];

  for (const { firstInput, secondInput, expected } of testData) {
    await doTest(async browser => {
      await openPopup(firstInput);
      await doEnter();

      await UrlbarTestUtils.promisePopupOpen(window, () => {
        EventUtils.synthesizeKey("l", { accelKey: true });
      });
      if (secondInput) {
        for (let i = 0; i < secondInput.length; i++) {
          EventUtils.synthesizeKey(secondInput.charAt(i));
        }
      }
      await UrlbarTestUtils.promiseSearchComplete(window);
      await doBlur();

      assertAbandonmentTelemetry([{ interaction: expected }]);
    });
  }
});

add_task(
  async function interaction_persisted_search_terms_restarted_refined_via_abandonment() {
    const testData = [
      {
        firstInput: "x",
        // Just move the focus to the URL bar after blur.
        secondInput: null,
        expected: "persisted_search_terms",
      },
      {
        firstInput: "x",
        secondInput: "x",
        expected: "persisted_search_terms",
      },
      {
        firstInput: "x",
        secondInput: "y",
        expected: "persisted_search_terms_restarted",
      },
      {
        firstInput: "x",
        secondInput: "x y",
        expected: "persisted_search_terms_refined",
      },
      {
        firstInput: "x y",
        secondInput: "x",
        expected: "persisted_search_terms_refined",
      },
    ];

    for (const { firstInput, secondInput, expected } of testData) {
      await doTest(async browser => {
        await openPopup("any search");
        await doEnter();

        await openPopup(firstInput);
        await doBlur();

        await UrlbarTestUtils.promisePopupOpen(window, () => {
          EventUtils.synthesizeKey("l", { accelKey: true });
        });
        if (secondInput) {
          for (let i = 0; i < secondInput.length; i++) {
            EventUtils.synthesizeKey(secondInput.charAt(i));
          }
        }
        await UrlbarTestUtils.promiseSearchComplete(window);
        await doBlur();

        assertAbandonmentTelemetry([
          { interaction: "persisted_search_terms_restarted" },
          { interaction: expected },
        ]);
      });
    }
  }
);
