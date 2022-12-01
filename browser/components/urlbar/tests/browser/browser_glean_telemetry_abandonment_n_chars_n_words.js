/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - n_chars
// - n_words

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();
});

add_task(async function n_chars() {
  for (const input of ["x", "xx", "xx x", "xx x "]) {
    await doTest(async browser => {
      await openPopup(input);
      await doBlur();

      assertAbandonmentTelemetry([{ n_chars: input.length }]);
    });
  }

  await doTest(async browser => {
    let input = "";
    for (let i = 0; i < UrlbarUtils.MAX_TEXT_LENGTH * 2; i++) {
      input += "x";
    }

    await openPopup(input);
    await doBlur();

    assertAbandonmentTelemetry([{ n_chars: UrlbarUtils.MAX_TEXT_LENGTH * 2 }]);
  });
});

add_task(async function n_words() {
  for (const input of ["x", "xx", "xx x", "xx x "]) {
    await doTest(async browser => {
      await openPopup(input);
      await doBlur();

      const splits = input.trim().split(" ");
      assertAbandonmentTelemetry([{ n_words: splits.length }]);
    });
  }

  await doTest(async browser => {
    const word = "1234 ";
    let input = "";
    while (input.length < UrlbarUtils.MAX_TEXT_LENGTH * 2) {
      input += word;
    }

    await openPopup(input);
    await doBlur();

    assertAbandonmentTelemetry([
      { n_words: UrlbarUtils.MAX_TEXT_LENGTH / word.length },
    ]);
  });
});
