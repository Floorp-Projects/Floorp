/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly in regards to case sensitivity.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html><p>test completion perfect match.
  <script>
    x = Object.create(null, Object.getOwnPropertyDescriptors({
      foo: 1,
      foO: 2,
      fOo: 3,
      fOO: 4,
    }));
  </script>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  info("Check that filtering the cache works like on the server");
  await setInputValueForAutocompletion(hud, "x.");
  ok(
    hasExactPopupLabels(autocompletePopup, ["foo", "foO", "fOo", "fOO"]),
    "popup has expected item, in expected order"
  );

  const onAutoCompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("foO");
  await onAutoCompleteUpdated;
  ok(
    hasExactPopupLabels(autocompletePopup, ["foO", "foo", "fOo", "fOO"]),
    "popup has expected item, in expected order"
  );

  info("Close autocomplete popup");
  await closeAutocompletePopup(hud);
});
