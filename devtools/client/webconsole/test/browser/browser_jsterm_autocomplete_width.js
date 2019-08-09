/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the autocomplete popup is resized when needed.

const TEST_URI = `data:text/html;charset=utf-8,
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

add_task(async function() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  const { autocompletePopup: popup } = jsterm;

  const onPopUpOpen = popup.once("popup-opened");

  info(`wait for completion suggestions for "xx"`);
  EventUtils.sendString("xx");

  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");

  const expectedPopupItems = ["xx", "xxx"];
  is(
    popup.items.map(i => i.label).join("-"),
    expectedPopupItems.join("-"),
    "popup has expected items"
  );

  const originalWidth = popup._tooltip.container.clientWidth;
  ok(
    originalWidth > 2 * jsterm._inputCharWidth,
    "popup is at least wider than the width of the longest list item"
  );

  info(`wait for completion suggestions for "xx."`);
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(".");
  await onAutocompleteUpdated;

  is(
    popup.items.map(i => i.label).join("-"),
    ["y".repeat(10), "z".repeat(20)].join("-"),
    "popup has expected items"
  );
  const newPopupWidth = popup._tooltip.container.clientWidth;
  ok(newPopupWidth > originalWidth, "The popup width was updated");
  ok(
    newPopupWidth > 20 * jsterm._inputCharWidth,
    "popup is at least wider than the width of the longest list item"
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
});
