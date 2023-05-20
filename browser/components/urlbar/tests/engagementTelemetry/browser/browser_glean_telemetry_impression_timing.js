/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the taking timing for the impression telemetry.

add_setup(async function () {
  await setup();
});

add_task(async function cancelImpressionTimerByEngagementEvent() {
  const additionalInterval = 1000;
  const originalInterval = UrlbarPrefs.get(
    "searchEngagementTelemetry.pauseImpressionIntervalMs"
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.urlbar.searchEngagementTelemetry.pauseImpressionIntervalMs",
        originalInterval + additionalInterval,
      ],
    ],
  });

  for (const trigger of [doEnter, doBlur]) {
    await doTest(async browser => {
      await openPopup("https://example.com");
      await trigger();

      // Check whether the impression timer was canceled.
      await new Promise(r =>
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(r, originalInterval + additionalInterval)
      );
      assertImpressionTelemetry([]);
    });
  }

  await SpecialPowers.popPrefEnv();
});

add_task(async function cancelInpressionTimerByType() {
  const originalInterval = UrlbarPrefs.get(
    "searchEngagementTelemetry.pauseImpressionIntervalMs"
  );

  await doTest(async browser => {
    await openPopup("x");
    await new Promise(r =>
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(r, originalInterval / 10)
    );
    assertImpressionTelemetry([]);

    EventUtils.synthesizeKey(" ");
    EventUtils.synthesizeKey("z");
    await UrlbarTestUtils.promiseSearchComplete(window);
    assertImpressionTelemetry([]);
    await waitForPauseImpression();

    assertImpressionTelemetry([{ n_chars: 3 }]);
  });
});

add_task(async function oneImpressionInOneSession() {
  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();

    // Sanity check.
    assertImpressionTelemetry([{ n_chars: 1 }]);

    // Add a keyword to start new query.
    EventUtils.synthesizeKey(" ");
    EventUtils.synthesizeKey("z");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await waitForPauseImpression();

    // No more taking impression telemetry.
    assertImpressionTelemetry([{ n_chars: 1 }]);

    // Finish the current session.
    await doEnter();

    // Should take pause impression since new session started.
    await openPopup("x z y");
    await waitForPauseImpression();
    assertImpressionTelemetry([{ n_chars: 1 }, { n_chars: 5 }]);
  });
});
