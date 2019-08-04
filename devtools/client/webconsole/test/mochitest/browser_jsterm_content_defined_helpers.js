/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that using helper functions in jsterm call the global content functions
// if they are defined.

const PREFIX = "content-";
const HELPERS = [
  "$_",
  "$",
  "$$",
  "$0",
  "$x",
  "cd",
  "clear",
  "clearHistory",
  "copy",
  "help",
  "inspect",
  "keys",
  "pprint",
  "screenshot",
  "values",
];

// The page script sets a global function for each known helper (except print).
const TEST_URI = `data:text/html,<meta charset=utf8>
  <script>
    const helpers = ${JSON.stringify(HELPERS)};
    for (const helper of helpers) {
      window[helper] = () => "${PREFIX}" + helper;
    }
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  for (const helper of HELPERS) {
    await setInputValueForAutocompletion(hud, helper);
    const autocompleteItems = getPopupLabels(autocompletePopup).filter(
      l => l === helper
    );
    is(
      autocompleteItems.length,
      1,
      `There's no duplicated "${helper}" item in the autocomplete popup`
    );

    await executeAndWaitForMessage(
      hud,
      `${helper}()`,
      `"${PREFIX + helper}"`,
      ".result"
    );
    ok(true, `output is correct for ${helper}()`);
  }
});

function getPopupLabels(popup) {
  return popup.getItems().map(item => item.label);
}
