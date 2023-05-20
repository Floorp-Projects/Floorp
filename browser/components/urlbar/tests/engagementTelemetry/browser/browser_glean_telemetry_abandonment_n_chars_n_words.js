/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - n_chars
// - n_words

add_setup(async function () {
  await initNCharsAndNWordsTest();
});

add_task(async function n_chars() {
  await doNCharsTest({
    trigger: () => doBlur(),
    assert: nChars => assertAbandonmentTelemetry([{ n_chars: nChars }]),
  });

  await doNCharsWithOverMaxTextLengthCharsTest({
    trigger: () => doBlur(),
    assert: nChars => assertAbandonmentTelemetry([{ n_chars: nChars }]),
  });
});

add_task(async function n_words() {
  await doNWordsTest({
    trigger: () => doBlur(),
    assert: nWords => assertAbandonmentTelemetry([{ n_words: nWords }]),
  });

  await doNWordsWithOverMaxTextLengthCharsTest({
    trigger: () => doBlur(),
    assert: nWords => assertAbandonmentTelemetry([{ n_words: nWords }]),
  });
});
