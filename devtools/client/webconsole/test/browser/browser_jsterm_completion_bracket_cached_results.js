/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly with `[` and cached results

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<p>test [ completion cached results.
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
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  info("Test that the autocomplete cache works with brackets");
  const { autocompletePopup } = jsterm;

  const tests = [
    {
      description: "Test that it works if the user did not type a quote",
      initialInput: `window.testObject[dat`,
      expectedItems: [`"data-test"`, `"dataTest"`, `"DATA-TEST"`],
      expectedCompletionText: `a-test"]`,
      sequence: [
        {
          char: "a",
          expectedItems: [`"data-test"`, `"dataTest"`, `"DATA-TEST"`],
          expectedCompletionText: `-test"]`,
        },
        {
          char: "-",
          expectedItems: [`"data-test"`, `"DATA-TEST"`],
          expectedCompletionText: `test"]`,
        },
        {
          char: "t",
          expectedItems: [`"data-test"`, `"DATA-TEST"`],
          expectedCompletionText: `est"]`,
        },
        {
          char: "e",
          expectedItems: [`"data-test"`, `"DATA-TEST"`],
          expectedCompletionText: `st"]`,
        },
      ],
    },
    {
      description: "Test that it works if the user did type a quote",
      initialInput: `window.testObject['dat`,
      expectedItems: [`'data-test'`, `'dataTest'`, `'DATA-TEST'`],
      expectedCompletionText: `a-test']`,
      sequence: [
        {
          char: "a",
          expectedItems: [`'data-test'`, `'dataTest'`, `'DATA-TEST'`],
          expectedCompletionText: `-test']`,
        },
        {
          char: "-",
          expectedItems: [`'data-test'`, `'DATA-TEST'`],
          expectedCompletionText: `test']`,
        },
        {
          char: "t",
          expectedItems: [`'data-test'`, `'DATA-TEST'`],
          expectedCompletionText: `est']`,
        },
        {
          char: "e",
          expectedItems: [`'data-test'`, `'DATA-TEST'`],
          expectedCompletionText: `st']`,
        },
      ],
    },
  ];

  for (const test of tests) {
    info(test.description);

    const onPopUpOpen = autocompletePopup.once("popup-opened");
    EventUtils.sendString(test.initialInput);
    await onPopUpOpen;

    is(
      getAutocompletePopupLabels(autocompletePopup).join("|"),
      test.expectedItems.join("|"),
      `popup has expected items, in expected order`
    );
    checkInputCompletionValue(
      hud,
      test.expectedCompletionText,
      `completeNode has expected value`
    );
    for (const {
      char,
      expectedItems,
      expectedCompletionText,
    } of test.sequence) {
      const onPopupUpdate = jsterm.once("autocomplete-updated");
      EventUtils.sendString(char);
      await onPopupUpdate;

      is(
        getAutocompletePopupLabels(autocompletePopup).join("|"),
        expectedItems.join("|"),
        `popup has expected items, in expected order`
      );
      checkInputCompletionValue(
        hud,
        expectedCompletionText,
        `completeNode has expected value`
      );
    }

    setInputValue(hud, "");
    const onPopupClose = autocompletePopup.once("popup-closed");
    EventUtils.synthesizeKey("KEY_Escape");
    await onPopupClose;
  }
});

function getAutocompletePopupLabels(autocompletePopup) {
  return autocompletePopup.items.map(i => i.label);
}
