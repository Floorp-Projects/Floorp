/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - interaction

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.urlbar.searchEngagementTelemetry.pauseImpressionIntervalMs",
        250,
      ],
    ],
  });
});

add_task(async function interaction_topsites() {
  await doTest(async browser => {
    await addTopSites("https://example.com/");
    await showResultByArrowDown();
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "topsites" }]);
  });
});

add_task(async function interaction_typed() {
  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "typed" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "typed" }]);
  });
});

add_task(async function interaction_pasted() {
  await doTest(async browser => {
    await doPaste("www.example.com");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "pasted" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    await doPaste("x");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "pasted" }]);
  });
});

add_task(async function interaction_topsite_search() {
  // TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=1804010
  // assertImpressionTelemetry([{ interaction: "topsite_search" }]);
});

add_task(async function interaction_returned_restarted_refined() {
  const testData = [
    {
      firstInput: "x",
      // Just move the focus to the URL bar after blur.
      secondInput: null,
      expected: "returned",
    },
    {
      firstInput: "x",
      secondInput: "x",
      expected: "returned",
    },
    {
      firstInput: "x",
      secondInput: "y",
      expected: "restarted",
    },
    {
      firstInput: "x",
      secondInput: "x y",
      expected: "refined",
    },
    {
      firstInput: "x y",
      secondInput: "x",
      expected: "refined",
    },
  ];

  for (const { firstInput, secondInput, expected } of testData) {
    await doTest(async browser => {
      await openPopup(firstInput);
      await waitForPauseImpression();
      await doBlur();

      await UrlbarTestUtils.promisePopupOpen(window, () => {
        document.getElementById("Browser:OpenLocation").doCommand();
      });
      if (secondInput) {
        for (let i = 0; i < secondInput.length; i++) {
          EventUtils.synthesizeKey(secondInput.charAt(i));
        }
      }
      await UrlbarTestUtils.promiseSearchComplete(window);
      await waitForPauseImpression();

      assertImpressionTelemetry([
        { reason: "pause" },
        { reason: "pause", interaction: expected },
      ]);
    });
  }
});

add_task(async function interaction_persisted_search_terms() {
  for (const showSearchTermsEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.showSearchTerms.featureGate", showSearchTermsEnabled],
        ["browser.urlbar.showSearchTerms.enabled", true],
        ["browser.search.widget.inNavBar", false],
      ],
    });

    await doTest(async browser => {
      await openPopup("x");
      await waitForPauseImpression();
      await doEnter();

      await openPopup("x");
      await waitForPauseImpression();

      assertImpressionTelemetry([
        { reason: "pause" },
        {
          interaction: showSearchTermsEnabled
            ? "persisted_search_terms"
            : "typed",
        },
      ]);
    });

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function interaction_persisted_search_terms_restarted_refined() {
  const testData = [
    {
      firstInput: "x",
      // Just move the focus to the URL bar after engagement.
      secondInput: null,
      expectedForShowSearchTermsEnabled: "persisted_search_terms",
      expectedForShowSearchTermsDisabled: "topsites",
    },
    {
      firstInput: "x",
      secondInput: "x",
      expectedForShowSearchTermsEnabled: "persisted_search_terms",
      expectedForShowSearchTermsDisabled: "typed",
    },
    {
      firstInput: "x",
      secondInput: "y",
      expectedForShowSearchTermsEnabled: "persisted_search_terms_restarted",
      expectedForShowSearchTermsDisabled: "typed",
    },
    {
      firstInput: "x",
      secondInput: "x y",
      expectedForShowSearchTermsEnabled: "persisted_search_terms_refined",
      expectedForShowSearchTermsDisabled: "typed",
    },
    {
      firstInput: "x y",
      secondInput: "x",
      expectedForShowSearchTermsEnabled: "persisted_search_terms_refined",
      expectedForShowSearchTermsDisabled: "typed",
    },
  ];

  for (const showSearchTermsEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.showSearchTerms.featureGate", showSearchTermsEnabled],
        ["browser.urlbar.showSearchTerms.enabled", true],
        ["browser.search.widget.inNavBar", false],
      ],
    });

    for (const {
      firstInput,
      secondInput,
      expectedForShowSearchTermsEnabled,
      expectedForShowSearchTermsDisabled,
    } of testData) {
      await doTest(async browser => {
        await openPopup(firstInput);
        await waitForPauseImpression();
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
        await waitForPauseImpression();

        assertImpressionTelemetry([
          { reason: "pause" },
          {
            interaction: showSearchTermsEnabled
              ? expectedForShowSearchTermsEnabled
              : expectedForShowSearchTermsDisabled,
          },
        ]);
      });
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(
  async function interaction_persisted_search_terms_restarted_refined_via_abandonment() {
    const testData = [
      {
        firstInput: "x",
        // Just move the focus to the URL bar after blur.
        secondInput: null,
        expectedForShowSearchTermsEnabled: "persisted_search_terms",
        expectedForShowSearchTermsDisabled: "returned",
      },
      {
        firstInput: "x",
        secondInput: "x",
        expectedForShowSearchTermsEnabled: "persisted_search_terms",
        expectedForShowSearchTermsDisabled: "returned",
      },
      {
        firstInput: "x",
        secondInput: "y",
        expectedForShowSearchTermsEnabled: "persisted_search_terms_restarted",
        expectedForShowSearchTermsDisabled: "restarted",
      },
      {
        firstInput: "x",
        secondInput: "x y",
        expectedForShowSearchTermsEnabled: "persisted_search_terms_refined",
        expectedForShowSearchTermsDisabled: "refined",
      },
      {
        firstInput: "x y",
        secondInput: "x",
        expectedForShowSearchTermsEnabled: "persisted_search_terms_refined",
        expectedForShowSearchTermsDisabled: "refined",
      },
    ];

    for (const showSearchTermsEnabled of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "browser.urlbar.showSearchTerms.featureGate",
            showSearchTermsEnabled,
          ],
          ["browser.urlbar.showSearchTerms.enabled", true],
          ["browser.search.widget.inNavBar", false],
        ],
      });

      for (const {
        firstInput,
        secondInput,
        expectedForShowSearchTermsEnabled,
        expectedForShowSearchTermsDisabled,
      } of testData) {
        await doTest(async browser => {
          await openPopup("any search");
          await waitForPauseImpression();
          await doEnter();

          await openPopup(firstInput);
          await waitForPauseImpression();
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
          await waitForPauseImpression();

          assertImpressionTelemetry([
            { reason: "pause" },
            { reason: "pause" },
            {
              interaction: showSearchTermsEnabled
                ? expectedForShowSearchTermsEnabled
                : expectedForShowSearchTermsDisabled,
            },
          ]);
        });
      }

      await SpecialPowers.popPrefEnv();
    }
  }
);
