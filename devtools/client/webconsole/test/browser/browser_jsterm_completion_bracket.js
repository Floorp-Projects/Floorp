/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly with `[`

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html><p>test [ completion.
  <script>
    window.testObject = Object.create(null, Object.getOwnPropertyDescriptors({
      bar: 0,
      dataTest: 1,
      "data-test": 2,
      'da"ta"test': 3,
      "da\`ta\`test": 4,
      "da'ta'test": 5,
      "DATA-TEST": 6,
      "DAT_\\\\a\\"'\`\${0}\\u0000\\b\\t\\n\\f\\r\\ude80\\ud83d\\ud83d\\ude80_TEST": 7,
    }));
  </script>`;

add_task(async function() {
  await pushPref("devtools.editor.autoclosebrackets", false);
  const hud = await openNewTabAndConsole(TEST_URI);
  await testInputs(hud, false);
  await testCompletionTextUpdateOnPopupNavigate(hud, false);
  await testAcceptCompletionExistingClosingBracket(hud);

  info("Test again with autoclosebracket set to true");
  await pushPref("devtools.editor.autoclosebrackets", true);
  const hudAutoclose = await openNewTabAndConsole(TEST_URI);
  await testInputs(hudAutoclose, true);
  await testCompletionTextUpdateOnPopupNavigate(hudAutoclose, true);
  await testAcceptCompletionExistingClosingBracket(hudAutoclose);
});

async function testInputs(hud, autocloseEnabled) {
  const tests = [
    {
      description: "Check that the popup is opened when typing `[`",
      input: "window.testObject[",
      expectedItems: [
        `"bar"`,
        `"da'ta'test"`,
        `"da\\"ta\\"test"`,
        `"da\`ta\`test"`,
        `"data-test"`,
        `"dataTest"`,
        `"DAT_\\\\a\\"'\`\${0}\\u0000\\b\\t\\n\\f\\r\\ude80\\ud83d🚀_TEST"`,
        `"DATA-TEST"`,
      ],
      expectedCompletionText: autocloseEnabled ? "" : `"bar"]`,
      expectedInputAfterCompletion: `window.testObject["bar"]`,
    },
    {
      description: "Test that the list can be filtered even without quote",
      input: "window.testObject[d",
      expectedItems: [
        `"da'ta'test"`,
        `"da\\"ta\\"test"`,
        `"da\`ta\`test"`,
        `"data-test"`,
        `"dataTest"`,
        `"DAT_\\\\a\\"'\`\${0}\\u0000\\b\\t\\n\\f\\r\\ude80\\ud83d🚀_TEST"`,
        `"DATA-TEST"`,
      ],
      expectedCompletionText: autocloseEnabled ? "" : `a'ta'test"]`,
      expectedInputAfterCompletion: `window.testObject["da'ta'test"]`,
    },
    {
      description: "Test filtering with quote and string",
      input: `window.testObject["d`,
      expectedItems: [
        `"da'ta'test"`,
        `"da\\"ta\\"test"`,
        `"da\`ta\`test"`,
        `"data-test"`,
        `"dataTest"`,
        `"DAT_\\\\a\\"'\`\${0}\\u0000\\b\\t\\n\\f\\r\\ude80\\ud83d🚀_TEST"`,
        `"DATA-TEST"`,
      ],
      expectedCompletionText: autocloseEnabled ? "" : `a'ta'test"]`,
      expectedInputAfterCompletion: `window.testObject["da'ta'test"]`,
    },
    {
      description: "Test filtering with simple quote and string",
      input: `window.testObject['d`,
      expectedItems: [
        `'da"ta"test'`,
        `'da\\'ta\\'test'`,
        `'da\`ta\`test'`,
        `'data-test'`,
        `'dataTest'`,
        `'DAT_\\\\a"\\'\`\${0}\\u0000\\b\\t\\n\\f\\r\\ude80\\ud83d🚀_TEST'`,
        `'DATA-TEST'`,
      ],
      expectedCompletionText: autocloseEnabled ? "" : `a"ta"test']`,
      expectedInputAfterCompletion: `window.testObject['da"ta"test']`,
    },
    {
      description: "Test filtering with template literal and string",
      input: "window.testObject[`d",
      expectedItems: [
        "`da'ta'test`",
        '`da"ta"test`',
        "`da\\`ta\\`test`",
        "`data-test`",
        "`dataTest`",
        "`DAT_\\\\a\"'\\`\\${0}\\u0000\\b\\t\\n\\f\\r\\ude80\\ud83d🚀_TEST`",
        "`DATA-TEST`",
      ],
      expectedCompletionText: autocloseEnabled ? "" : "a'ta'test`]",
      expectedInputAfterCompletion: "window.testObject[`da'ta'test`]",
    },
    {
      description: "Test that filtering is case insensitive",
      input: "window.testObject[data-t",
      expectedItems: [`"data-test"`, `"DATA-TEST"`],
      expectedCompletionText: autocloseEnabled ? "" : `est"]`,
      expectedInputAfterCompletion: `window.testObject["data-test"]`,
    },
    {
      description:
        "Test that filtering without quote displays the popup when there's only 1 match",
      input: "window.testObject[DATA-",
      expectedItems: [`"DATA-TEST"`],
      expectedCompletionText: autocloseEnabled ? "" : `TEST"]`,
      expectedInputAfterCompletion: `window.testObject["DATA-TEST"]`,
    },
  ];

  for (const test of tests) {
    await testInput(hud, test);
  }
}

async function testInput(
  hud,
  {
    description,
    input,
    expectedItems,
    expectedCompletionText,
    expectedInputAfterCompletion,
  }
) {
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  info(`${description} - test popup opening`);
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString(input);
  await onPopUpOpen;

  ok(
    hasExactPopupLabels(autocompletePopup, expectedItems),
    `${description} - popup has expected item, in expected order`
  );
  checkInputCompletionValue(
    hud,
    expectedCompletionText,
    `${description} - completeNode has expected value`
  );

  info(`${description} - test accepting completion`);
  const onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkInputValueAndCursorPosition(
    hud,
    expectedInputAfterCompletion + "|",
    `${description} - input was completed as expected`
  );
  checkInputCompletionValue(hud, "", `${description} - completeNode is empty`);

  setInputValue(hud, "");
}

async function testCompletionTextUpdateOnPopupNavigate(hud, autocloseEnabled) {
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  info(
    "Test that navigating the popup list update the completionText as expected"
  );
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  const input = `window.testObject[data`;
  EventUtils.sendString(input);
  await onPopUpOpen;

  ok(
    hasExactPopupLabels(autocompletePopup, [
      `"data-test"`,
      `"dataTest"`,
      `"DATA-TEST"`,
    ]),
    `popup has expected items, in expected order`
  );
  checkInputCompletionValue(
    hud,
    autocloseEnabled ? "" : `-test"]`,
    `completeNode has expected value`
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInputCompletionValue(
    hud,
    autocloseEnabled ? "" : `Test"]`,
    `completeNode has expected value`
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInputCompletionValue(
    hud,
    autocloseEnabled ? "" : `-TEST"]`,
    `completeNode has expected value`
  );

  const onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkInputValueAndCursorPosition(
    hud,
    `window.testObject["DATA-TEST"]|`,
    `input was completed as expected after navigating the popup`
  );
}

async function testAcceptCompletionExistingClosingBracket(hud) {
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  info(
    "Check that accepting completion when there's a closing bracket does not append " +
      "another closing bracket"
  );
  await setInputValueForAutocompletion(hud, "window.testObject[]", -1);
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString(`"b`);
  await onPopUpOpen;
  ok(
    hasExactPopupLabels(autocompletePopup, [`"bar"`]),
    `popup has expected item`
  );

  const onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkInputValueAndCursorPosition(
    hud,
    `window.testObject["bar"]|`,
    `input was completed as expected, without adding a closing bracket`
  );
}
