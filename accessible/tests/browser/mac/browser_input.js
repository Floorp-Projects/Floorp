/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function testInput(browser, accDoc) {
  let input = getNativeInterface(accDoc, "input");

  is(input.getAttributeValue("AXDescription"), "Name", "Correct input label");
  is(input.getAttributeValue("AXTitle"), "", "Correct input title");
  is(input.getAttributeValue("AXValue"), "Elmer Fudd", "Correct input value");
  is(
    input.getAttributeValue("AXNumberOfCharacters"),
    10,
    "Correct length of value"
  );

  ok(input.attributeNames.includes("AXSelectedText"), "Has AXSelectedText");
  ok(
    input.attributeNames.includes("AXSelectedTextRange"),
    "Has AXSelectedTextRange"
  );

  let evt = Promise.all([
    waitForMacEvent("AXFocusedUIElementChanged", "input"),
    waitForMacEvent("AXSelectedTextChanged", "body"),
    waitForMacEvent("AXSelectedTextChanged", "input"),
  ]);
  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("input").focus();
  });
  await evt;

  evt = Promise.all([
    waitForMacEvent("AXSelectedTextChanged", "body"),
    waitForMacEvent("AXSelectedTextChanged", "input"),
  ]);
  await SpecialPowers.spawn(browser, [], () => {
    let elm = content.document.getElementById("input");
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
  await evt;

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
    let elm = content.document.querySelector("input#input");
    return elm.value.substring(elm.selectionStart, elm.selectionEnd);
  });

  is(domSelection, "lmer Fu", "correct DOM selection");

  is(
    input.getParameterizedAttributeValue("AXStringForRange", NSRange(3, 5)),
    "er Fu",
    "AXStringForRange works"
  );
}

/**
 * Input selection test
 */
addAccessibleTask(
  `<input aria-label="Name" id="input" value="Elmer Fudd">`,
  testInput
);
