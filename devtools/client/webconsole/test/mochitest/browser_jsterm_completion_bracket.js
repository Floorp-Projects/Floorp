/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly with `[`

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<p>test [ completion.
  <script>
    window.testObject = Object.create(null, Object.getOwnPropertyDescriptors({
      bar: 0,
      dataTest: 1,
      "data-test": 2,
      'da"ta"test': 3,
      "da\`ta\`test": 4,
      "da'ta'test": 5,
      "DATA-TEST": 6,
    }));
  </script>`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm} = await openNewTabAndConsole(TEST_URI);

  await testInputs(jsterm);
  await testCompletionTextUpdateOnPopupNavigate(jsterm);
  await testAcceptCompletionExistingClosingBracket(jsterm);
}

async function testInputs(jsterm) {
  const tests = [{
    description: "Check that the popup is opened when typing `[`",
    input: "window.testObject[",
    expectedItems: [
      `"bar"`,
      `"da'ta'test"`,
      `"da\\"ta\\"test"`,
      `"da\`ta\`test"`,
      `"data-test"`,
      `"dataTest"`,
      `"DATA-TEST"`,
    ],
    expectedCompletionText: `"bar"]`,
    expectedInputAfterCompletion: `window.testObject["bar"]`
  }, {
    description: "Test that the list can be filtered even without quote",
    input: "window.testObject[d",
    expectedItems: [
      `"da'ta'test"`,
      `"da\\"ta\\"test"`,
      `"da\`ta\`test"`,
      `"data-test"`,
      `"dataTest"`,
      `"DATA-TEST"`,
    ],
    expectedCompletionText: `a'ta'test"]`,
    expectedInputAfterCompletion: `window.testObject["da'ta'test"]`,
  }, {
    description: "Test filtering with quote and string",
    input: `window.testObject["d`,
    expectedItems: [
      `"da'ta'test"`,
      `"da\\"ta\\"test"`,
      `"da\`ta\`test"`,
      `"data-test"`,
      `"dataTest"`,
      `"DATA-TEST"`,
    ],
    expectedCompletionText: `a'ta'test"]`,
    expectedInputAfterCompletion: `window.testObject["da'ta'test"]`,
  }, {
    description: "Test filtering with simple quote and string",
    input: `window.testObject['d`,
    expectedItems: [
      `'da"ta"test'`,
      `'da\\'ta\\'test'`,
      `'da\`ta\`test'`,
      `'data-test'`,
      `'dataTest'`,
      `'DATA-TEST'`,
    ],
    expectedCompletionText: `a"ta"test']`,
    expectedInputAfterCompletion: `window.testObject['da"ta"test']`,
  }, {
    description: "Test filtering with template literal and string",
    input: "window.testObject[`d",
    expectedItems: [
      "`da\"ta\"test`",
      "`da'ta'test`",
      "`da\\`ta\\`test`",
      "`data-test`",
      "`dataTest`",
      "`DATA-TEST`",
    ],
    expectedCompletionText: 'a"ta"test`]',
    expectedInputAfterCompletion: 'window.testObject[`da"ta"test`]',
  }, {
    description: "Test that filtering is case insensitive",
    input: "window.testObject[data-t",
    expectedItems: [
      `"data-test"`,
      `"DATA-TEST"`,
    ],
    expectedCompletionText: `est"]`,
    expectedInputAfterCompletion: `window.testObject["data-test"]`,
  }, {
    description:
      "Test that filtering without quote displays the popup when there's only 1 match",
    input: "window.testObject[DATA-",
    expectedItems: [
      `"DATA-TEST"`
    ],
    expectedCompletionText: `TEST"]`,
    expectedInputAfterCompletion: `window.testObject["DATA-TEST"]`,
  }];

  for (const test of tests) {
    await testInput(jsterm, test);
  }
}

async function testInput(jsterm, {
  description,
  input,
  expectedItems,
  expectedCompletionText,
  expectedInputAfterCompletion
}) {
  const {autocompletePopup} = jsterm;

  info(`${description} - test popup opening`);
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString(input);
  await onPopUpOpen;

  is(getAutocompletePopupLabels(autocompletePopup).join("|"), expectedItems.join("|"),
    `${description} - popup has expected item, in expected order`);
  checkJsTermCompletionValue(jsterm, " ".repeat(input.length) + expectedCompletionText,
    `${description} - completeNode has expected value`);

  info(`${description} - test accepting completion`);
  const onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkJsTermValueAndCursor(jsterm, expectedInputAfterCompletion + "|",
    `${description} - input was completed as expected`);
  checkJsTermCompletionValue(jsterm, "", `${description} - completeNode is empty`);

  jsterm.setInputValue("");
}

async function testCompletionTextUpdateOnPopupNavigate(jsterm) {
  const {autocompletePopup} = jsterm;

  info("Test that navigating the popup list update the completionText as expected");
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  const input = `window.testObject[data`;
  EventUtils.sendString(input);
  await onPopUpOpen;

  is(getAutocompletePopupLabels(autocompletePopup).join("|"),
    `"data-test"|"dataTest"|"DATA-TEST"`, `popup has expected items, in expected order`);
  checkJsTermCompletionValue(jsterm, " ".repeat(input.length) + `-test"]`,
    `completeNode has expected value`);

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkJsTermCompletionValue(jsterm, " ".repeat(input.length) + `Test"]`,
    `completeNode has expected value`);

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkJsTermCompletionValue(jsterm, " ".repeat(input.length) + `-TEST"]`,
    `completeNode has expected value`);

  const onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkJsTermValueAndCursor(jsterm, `window.testObject["DATA-TEST"]|`,
    `input was completed as expected after navigating the popup`);
}

async function testAcceptCompletionExistingClosingBracket(jsterm) {
  const {autocompletePopup} = jsterm;

  info("Check that accepting completion when there's a closing bracket does not append " +
    "another closing bracket");
  await setInputValueForAutocompletion(jsterm, "window.testObject[]", -1);
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString("b");
  await onPopUpOpen;
  is(getAutocompletePopupLabels(autocompletePopup).join("|"), `"bar"`,
    `popup has expected item`);

  const onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkJsTermValueAndCursor(jsterm, `window.testObject["bar"|]`,
    `input was completed as expected, without adding a closing bracket`);
}

function getAutocompletePopupLabels(autocompletePopup) {
  return autocompletePopup.items.map(i => i.label);
}
