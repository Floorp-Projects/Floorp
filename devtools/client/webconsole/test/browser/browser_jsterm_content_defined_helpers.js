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
  "clear",
  "clearHistory",
  "copy",
  "help",
  "inspect",
  "keys",
  "screenshot",
  "values",
];

// The page script sets a global function for each known helper (except print).
const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8>
  <script>
    const helpers = ${JSON.stringify(HELPERS)};
    for (const helper of helpers) {
      window[helper] = () => "${PREFIX}" + helper;
    }
  </script>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  for (const helper of HELPERS) {
    await setInputValueForAutocompletion(hud, helper);
    const autocompleteItems = getAutocompletePopupLabels(
      autocompletePopup
    ).filter(l => l === helper);
    is(
      autocompleteItems.length,
      1,
      `There's no duplicated "${helper}" item in the autocomplete popup`
    );

    await executeAndWaitForResultMessage(
      hud,
      `${helper}()`,
      `"${PREFIX + helper}"`
    );
    ok(true, `output is correct for ${helper}()`);
  }
});
