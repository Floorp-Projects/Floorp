/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests unit conversion on browser.
 */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.unitConversion.enabled", true],
      // There are cases that URLBar loses focus before assertion of this test.
      // In that case, this test will be failed since the result is closed
      // before it. We use this pref so that keep the result even if lose focus.
      ["ui.popup.disable_autohide", true],
    ],
  });

  registerCleanupFunction(function() {
    SpecialPowers.clipboardCopyString("");
  });
});

add_task(async function test_selectByMouse() {
  // Clear clipboard content.
  SpecialPowers.clipboardCopyString("");

  const row = await doUnitConversion();

  info("Check if the result is copied to clipboard when selecting by mouse");
  EventUtils.synthesizeMouseAtCenter(
    row.querySelector(".urlbarView-no-wrap"),
    {},
    window
  );
  assertClipboard();

  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function test_selectByKey() {
  // Clear clipboard content.
  SpecialPowers.clipboardCopyString("");

  await doUnitConversion();

  // As gURLBar might lost focus,
  // give focus again in order to enable key event on the result.
  gURLBar.focus();

  info("Check if the result is copied to clipboard when selecting by key");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  assertClipboard();

  await UrlbarTestUtils.promisePopupClose(window);
});

function assertClipboard() {
  Assert.equal(
    SpecialPowers.getClipboardData("text/unicode"),
    "100 cm",
    "The result of conversion is copied to clipboard"
  );
}

async function doUnitConversion() {
  info("Do unit conversion then wait the result");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "1m to cm",
    waitForFocus: SimpleTest.waitForFocus,
  });

  const row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);

  Assert.ok(row.querySelector(".urlbarView-favicon"), "The icon is displayed");
  Assert.equal(
    row.querySelector(".urlbarView-dynamic-unitConversion-output").textContent,
    "100 cm",
    "The unit is converted"
  );

  return row;
}
