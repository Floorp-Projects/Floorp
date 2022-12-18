/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of engagement telemetry.
// - interaction

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();
});

add_task(async function interaction_topsites() {
  await doTest(async browser => {
    await addTopSites("https://example.com/");
    await showResultByArrowDown();
    await selectRowByURL("https://example.com/");
    await doEnter();

    assertEngagementTelemetry([{ interaction: "topsites" }]);
  });
});

add_task(async function interaction_typed() {
  await doTest(async browser => {
    await openPopup("x");
    await doEnter();

    assertEngagementTelemetry([{ interaction: "typed" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await doEnter();

    assertEngagementTelemetry([{ interaction: "typed" }]);
  });
});

add_task(async function interaction_dropped() {
  await doTest(async browser => {
    await doDropAndGo("example.com");

    assertEngagementTelemetry([{ interaction: "dropped" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    await doDropAndGo("example.com");

    assertEngagementTelemetry([{ interaction: "dropped" }]);
  });
});

add_task(async function interaction_pasted() {
  await doTest(async browser => {
    await doPaste("www.example.com");
    await doEnter();

    assertEngagementTelemetry([{ interaction: "pasted" }]);
  });

  await doTest(async browser => {
    await doPasteAndGo("www.example.com");

    assertEngagementTelemetry([{ interaction: "pasted" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    await doPaste("x");
    await doEnter();

    assertEngagementTelemetry([{ interaction: "pasted" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    await doPasteAndGo("www.example.com");

    assertEngagementTelemetry([{ interaction: "pasted" }]);
  });
});

add_task(async function interaction_topsite_search() {
  // TODO
  // assertEngagementTelemetry([{ interaction: "topsite_search" }]);
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
      await doEnter();

      assertEngagementTelemetry([{ interaction: expected }]);
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
      await doEnter();

      await openPopup("x");
      await doEnter();

      assertEngagementTelemetry([
        { interaction: "typed" },
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
        await doEnter();

        assertEngagementTelemetry([
          { interaction: "typed" },
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
          await doEnter();

          assertEngagementTelemetry([
            { interaction: "typed" },
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
