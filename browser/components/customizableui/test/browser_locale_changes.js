/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the novel Fluent behavior that it properly changes the language when the locale
 * is changed.
 */
add_task(async function testChangingLocaleOpen() {
  if (Services.locale.requestedLocale != "en-US") {
    ok(true, "The requested locale is not en-US, skipping this test.");
    return;
  }
  const enUsTitle = "Print";
  const accentedTitle = "[Ƥřiƞŧ]";
  startCustomizing();

  function getPrintButton() {
    return TestUtils.waitForCondition(
      () => document.getElementById("wrapper-print-button"),
      "Get the print button"
    );
  }

  info("Finding the Print button.");
  let printButton = await getPrintButton();

  await TestUtils.waitForCondition(
    () => printButton.getAttribute("title"),
    "Wait for the initial translation to happen."
  );

  // This assertion could begin failing if localization file is updated. In this case,
  // update this test to a new value.
  Assert.equal(
    printButton.getAttribute("title"),
    enUsTitle,
    "The button says Print."
  );
  Assert.equal(
    printButton.getAttribute("tooltiptext"),
    enUsTitle,
    "The tooltip says Print."
  );

  info("Switch to the accented pseudo locale.");
  await SpecialPowers.pushPrefEnv({ set: [["intl.l10n.pseudo", "accented"]] });

  await TestUtils.waitForCondition(
    () => printButton.getAttribute("title") == accentedTitle,
    "Wait for the accented translation to happen."
  );

  Assert.equal(
    printButton.getAttribute("title"),
    accentedTitle,
    "The button says Print."
  );
  Assert.equal(
    printButton.getAttribute("tooltiptext"),
    accentedTitle,
    "The tooltip says Print."
  );

  info("Switch the pseudo locale while the customization panel is closed.");
  await endCustomizing();
  await SpecialPowers.pushPrefEnv({ set: [["intl.l10n.pseudo", ""]] });
  await document.l10n.ready;
  startCustomizing();

  printButton = await getPrintButton();

  await TestUtils.waitForCondition(
    () => printButton.getAttribute("title") == enUsTitle,
    "Wait for the final en-US translation to happen."
  );

  Assert.equal(
    printButton.getAttribute("title"),
    enUsTitle,
    "The button says Print."
  );
  Assert.equal(
    printButton.getAttribute("tooltiptext"),
    enUsTitle,
    "The tooltip says Print."
  );

  await endCustomizing();
});
