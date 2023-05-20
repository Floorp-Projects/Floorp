/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - n_chars
// - n_words

add_setup(async function () {
  await initNCharsAndNWordsTest();
});

add_task(async function n_chars() {
  await doNCharsTest({
    trigger: () => waitForPauseImpression(),
    assert: nChars =>
      assertImpressionTelemetry([{ reason: "pause", n_chars: nChars }]),
  });

  await doNCharsWithOverMaxTextLengthCharsTest({
    trigger: () => waitForPauseImpression(),
    assert: nChars =>
      assertImpressionTelemetry([{ reason: "pause", n_chars: nChars }]),
  });
});

add_task(async function n_words() {
  await doNWordsTest({
    trigger: () => waitForPauseImpression(),
    assert: nWords =>
      assertImpressionTelemetry([{ reason: "pause", n_words: nWords }]),
  });

  await doNWordsWithOverMaxTextLengthCharsTest({
    trigger: () => waitForPauseImpression(),
    assert: nWords =>
      assertImpressionTelemetry([{ reason: "pause", n_words: nWords }]),
  });
});
