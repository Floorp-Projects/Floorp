/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Input selection test
 */
addAccessibleTask(
  `<input aria-label="Name" id="input" value="Elmer Fudd">`,
  async (browser, accDoc) => {
    let input = getNativeInterface(accDoc, "input");

    is(input.getAttributeValue("AXDescription"), "Name", "Correct input label");
    is(input.getAttributeValue("AXTitle"), "", "Correct input title");
    is(
      input.getAttributeValue("AXNumberOfCharacters"),
      10,
      "Correct length of value"
    );

    ok(input.attributeNames.includes("AXSelectedText"), "Has AXSelecetdText");
    ok(
      input.attributeNames.includes("AXSelectedTextRange"),
      "Has AXSelecetdTextRange"
    );

    // XXX: Need to look into why we send so many AXSelectedTextChanged notifications.
    let evt = Promise.all([
      waitForMacEvent("AXFocusedUIElementChanged"),
      waitForMacEvent("AXSelectedTextChanged"),
      waitForMacEvent("AXSelectedTextChanged"),
      waitForMacEvent("AXSelectedTextChanged"),
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      let elm = content.document.getElementById("input");
      elm.focus();
      elm.setSelectionRange(6, 9);
    });
    await evt;

    is(
      input.getAttributeValue("AXSelectedText"),
      "Fud",
      "Correct text is selected"
    );

    Assert.deepEqual(
      input.getAttributeValue("AXSelectedTextRange"),
      [6, 3],
      "correct range selected"
    );

    ok(
      input.isAttributeSettable("AXSelectedTextRange"),
      "AXSelectedTextRange is settable"
    );

    evt = Promise.all([
      waitForMacEvent("AXSelectedTextChanged"),
      waitForMacEvent("AXSelectedTextChanged"),
    ]);
    input.setAttributeValue("AXSelectedTextRange", NSRange(1, 7));

    Assert.deepEqual(
      input.getAttributeValue("AXSelectedTextRange"),
      [1, 7],
      "correct range selected"
    );

    is(
      input.getAttributeValue("AXSelectedText"),
      "lmer Fu",
      "Correct text is selected"
    );

    let domSelection = await SpecialPowers.spawn(browser, [], () => {
      let elm = content.document.getElementById("input");
      return [elm.selectionStart, elm.selectionEnd];
    });

    Assert.deepEqual(domSelection, [1, 8], "correct DOM selection");
  }
);
