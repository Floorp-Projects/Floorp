/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doTopsitesTest({ trigger, assert }) {
  await doTest(async browser => {
    await addTopSites("https://example.com/");

    await showResultByArrowDown();
    await selectRowByURL("https://example.com/");

    await trigger();
    await assert();
  });
}

async function doTypedTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");

    await trigger();
    await assert();
  });
}

async function doTypedWithResultsPopupTest({ trigger, assert }) {
  await doTest(async browser => {
    await showResultByArrowDown();
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);

    await trigger();
    await assert();
  });
}

async function doPastedTest({ trigger, assert }) {
  await doTest(async browser => {
    await doPaste("www.example.com");

    await trigger();
    await assert();
  });
}

async function doPastedWithResultsPopupTest({ trigger, assert }) {
  await doTest(async browser => {
    await showResultByArrowDown();
    await doPaste("x");

    await trigger();
    await assert();
  });
}

async function doReturnedRestartedRefinedTest({ trigger, assert }) {
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

      await trigger();
      await assert(expected);
    });
  }
}

async function doPersistedSearchTermsTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();
    await doEnter();

    await openPopup("x");

    await trigger();
    await assert();
  });
}

async function doPersistedSearchTermsRestartedRefinedTest({
  enabled,
  trigger,
  assert,
}) {
  const testData = [
    {
      firstInput: "x",
      // Just move the focus to the URL bar after engagement.
      secondInput: null,
      expected: enabled ? "persisted_search_terms" : "topsites",
    },
    {
      firstInput: "x",
      secondInput: "x",
      expected: enabled ? "persisted_search_terms" : "typed",
    },
    {
      firstInput: "x",
      secondInput: "y",
      expected: enabled ? "persisted_search_terms_restarted" : "typed",
    },
    {
      firstInput: "x",
      secondInput: "x y",
      expected: enabled ? "persisted_search_terms_refined" : "typed",
    },
    {
      firstInput: "x y",
      secondInput: "x",
      expected: enabled ? "persisted_search_terms_refined" : "typed",
    },
  ];

  for (const { firstInput, secondInput, expected } of testData) {
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

      await trigger();
      await assert(expected);
    });
  }
}

async function doPersistedSearchTermsRestartedRefinedViaAbandonmentTest({
  enabled,
  trigger,
  assert,
}) {
  const testData = [
    {
      firstInput: "x",
      // Just move the focus to the URL bar after blur.
      secondInput: null,
      expected: enabled ? "persisted_search_terms" : "returned",
    },
    {
      firstInput: "x",
      secondInput: "x",
      expected: enabled ? "persisted_search_terms" : "returned",
    },
    {
      firstInput: "x",
      secondInput: "y",
      expected: enabled ? "persisted_search_terms_restarted" : "restarted",
    },
    {
      firstInput: "x",
      secondInput: "x y",
      expected: enabled ? "persisted_search_terms_refined" : "refined",
    },
    {
      firstInput: "x y",
      secondInput: "x",
      expected: enabled ? "persisted_search_terms_refined" : "refined",
    },
  ];

  for (const { firstInput, secondInput, expected } of testData) {
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

      await trigger();
      await assert(expected);
    });
  }
}
