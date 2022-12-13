/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
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
    await doBlur();

    assertAbandonmentTelemetry([{ interaction: "topsites" }]);
  });
});

add_task(async function interaction_typed() {
  await doTest(async browser => {
    await openPopup("x");
    await doBlur();

    assertAbandonmentTelemetry([{ interaction: "typed" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await doBlur();

    assertAbandonmentTelemetry([{ interaction: "typed" }]);
  });
});

add_task(async function interaction_pasted() {
  await doTest(async browser => {
    await doPaste("www.example.com");
    await doBlur();

    assertAbandonmentTelemetry([{ interaction: "pasted" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    await doPaste("x");
    await doBlur();

    assertAbandonmentTelemetry([{ interaction: "pasted" }]);
  });
});

add_task(async function interaction_topsite_search() {
  // TODO
  // assertAbandonmentTelemetry([{ interaction: "topsite_search" }]);
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
      await doBlur();

      assertAbandonmentTelemetry([
        { interaction: "typed" },
        { interaction: expected },
      ]);
    });
  }
});

add_task(async function interaction_persisted_search_terms() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.showSearchTerms.featureGate", true],
      ["browser.urlbar.showSearchTerms.enabled", true],
      ["browser.search.widget.inNavBar", false],
    ],
  });

  await doTest(async browser => {
    await openPopup("keyword");
    await doEnter();

    await openPopup("keyword");
    await doBlur();

    assertAbandonmentTelemetry([{ interaction: "persisted_search_terms" }]);
  });

  await SpecialPowers.popPrefEnv();
});
