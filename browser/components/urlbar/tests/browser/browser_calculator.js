/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FORMULA = "8 * 8";
const RESULT = "64";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.calculator", true]],
  });
});

add_task(async function test_calculator() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: FORMULA,
  });

  let result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
  Assert.equal(result.payload.input, FORMULA);
  Assert.equal(result.payload.value, RESULT);

  EventUtils.synthesizeKey("KEY_ArrowDown");

  // Ensure the RESULT get written to the clipboard when selected.
  await SimpleTest.promiseClipboardChange(RESULT, () => {
    EventUtils.synthesizeKey("KEY_Enter");
  });
});
