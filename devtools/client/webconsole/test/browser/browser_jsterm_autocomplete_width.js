/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the autocomplete popup is resized when needed.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<head>
  <script>
    /* Create prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.xx = Object.create(null, Object.getOwnPropertyDescriptors({
      ["y".repeat(10)]: 1,
      ["z".repeat(20)]: 2
    }));
    window.xxx = 1;
  </script>
</head>
<body>Test</body>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup: popup } = jsterm;

  info(`wait for completion suggestions for "xx"`);
  await setInputValueForAutocompletion(hud, "xx");
  ok(popup.isOpen, "popup is open");

  const expectedPopupItems = ["xx", "xxx"];
  ok(
    hasExactPopupLabels(popup, expectedPopupItems),
    "popup has expected items"
  );

  const originalWidth = popup._tooltip.container.clientWidth;
  ok(
    originalWidth >= getLongestLabelWidth(jsterm),
    `popup (${originalWidth}px) is at least wider than the width of the longest list item (${getLongestLabelWidth(
      jsterm
    )}px)`
  );

  info(`wait for completion suggestions for "xx."`);
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(".");
  await onAutocompleteUpdated;

  ok(
    hasExactPopupLabels(popup, ["y".repeat(10), "z".repeat(20)]),
    "popup has expected items"
  );
  const newPopupWidth = popup._tooltip.container.clientWidth;
  ok(
    newPopupWidth >= originalWidth,
    `The popup width was updated (${originalWidth}px -> ${newPopupWidth}px)`
  );
  ok(
    newPopupWidth >= getLongestLabelWidth(jsterm),
    `popup (${newPopupWidth}px) is at least wider than the width of the longest list item (${getLongestLabelWidth(
      jsterm
    )}px)`
  );

  info(`wait for completion suggestions for "xx"`);
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Backspace");
  await onAutocompleteUpdated;

  is(
    popup._tooltip.container.clientWidth,
    originalWidth,
    "popup is back to its original width"
  );

  info("Close autocomplete popup");
  await closeAutocompletePopup(hud);
});

function getLongestLabelWidth(jsterm) {
  return (
    jsterm._inputCharWidth *
    getAutocompletePopupLabels(jsterm.autocompletePopup).sort(
      (a, b) => a < b
    )[0].length
  );
}
