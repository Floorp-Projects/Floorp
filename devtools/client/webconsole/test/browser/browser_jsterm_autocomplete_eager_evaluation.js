/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the main expression is eagerly evaluated and its results are used in the autocomple popup

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html>Test autocompletion for expression variables<script>
    var testObj = {
      fun: () => ({ yay: "yay", yo: "yo", boo: "boo" })
    };
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  const cases = [
    { input: "testObj.fun().y", results: ["yay", "yo"] },
    {
      input: `Array.of(1,2,3).reduce((i, agg) => agg + i).toS`,
      results: ["toString"],
    },
    { input: `1..toE`, results: ["toExponential"] },
  ];

  for (const test of cases) {
    info(`Test: ${test.input}`);
    await setInputValueForAutocompletion(hud, test.input);
    ok(
      hasExactPopupLabels(autocompletePopup, test.results),
      "Autocomplete popup shows expected results: " +
        getAutocompletePopupLabels(autocompletePopup).join("\n")
    );
  }
});
