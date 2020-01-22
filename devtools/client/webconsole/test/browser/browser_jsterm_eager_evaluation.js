/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
<script>
let x = 3, y = 4;
function zzyzx() {
  x = 10;
}
function zzyzx2() {
  x = 10;
}
</script>
`;

const EAGER_EVALUATION_PREF = "devtools.webconsole.input.eagerEvaluation";

// Basic testing of eager evaluation functionality. Expressions which can be
// eagerly evaluated should show their results, and expressions with side
// effects should not perform those side effects.
add_task(async function() {
  await pushPref(EAGER_EVALUATION_PREF, true);
  const hud = await openNewTabAndConsole(TEST_URI);

  // Do an evaluation to populate $_
  await executeAndWaitForMessage(
    hud,
    "'result: ' + (x + y)",
    "result: 7",
    ".result"
  );

  setInputValue(hud, "x + y");
  await waitForEagerEvaluationResult(hud, "7");

  setInputValue(hud, "x + y + undefined");
  await waitForEagerEvaluationResult(hud, "NaN");

  setInputValue(hud, "1 - 1");
  await waitForEagerEvaluationResult(hud, "0");

  setInputValue(hud, "!true");
  await waitForEagerEvaluationResult(hud, "false");

  setInputValue(hud, `"ab".slice(0, 0)`);
  await waitForEagerEvaluationResult(hud, `""`);

  setInputValue(hud, `JSON.parse("null")`);
  await waitForEagerEvaluationResult(hud, "null");

  setInputValue(hud, "-x / 0");
  await waitForEagerEvaluationResult(hud, "-Infinity");

  setInputValue(hud, "x = 10");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + 1");
  await waitForEagerEvaluationResult(hud, "4");

  setInputValue(hud, "zzyzx()");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + 2");
  await waitForEagerEvaluationResult(hud, "5");

  setInputValue(hud, "x +");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + z");
  await waitForEagerEvaluationResult(hud, /ReferenceError/);

  setInputValue(hud, "var a = 5");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + a");
  await waitForEagerEvaluationResult(hud, /ReferenceError/);

  setInputValue(hud, '"foobar".slice(1, 5)');
  await waitForEagerEvaluationResult(hud, '"ooba"');

  setInputValue(hud, '"foobar".toString()');
  await waitForEagerEvaluationResult(hud, '"foobar"');

  setInputValue(hud, "(new Array()).push(3)");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "(new Uint32Array([1,2,3])).includes(2)");
  await waitForEagerEvaluationResult(hud, "true");

  setInputValue(hud, "Math.round(3.2)");
  await waitForEagerEvaluationResult(hud, "3");

  info("Check that $_ wasn't polluted by eager evaluations");
  setInputValue(hud, "$_");
  await waitForEagerEvaluationResult(hud, `"result: 7"`);

  setInputValue(hud, "'> ' + $_");
  await waitForEagerEvaluationResult(hud, `"> result: 7"`);
});

// Test that the currently selected autocomplete result is eagerly evaluated.
add_task(async function() {
  await pushPref(EAGER_EVALUATION_PREF, true);
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  const { autocompletePopup: popup } = jsterm;

  ok(!popup.isOpen, "popup is not open");
  const onPopupOpen = popup.once("popup-opened");
  EventUtils.sendString("zzy");
  await onPopupOpen;

  await waitForEagerEvaluationResult(hud, "function zzyzx()");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await waitForEagerEvaluationResult(hud, "function zzyzx2()");
});

// Test that the setting works as expected.
add_task(async function() {
  // Settings is only enabled on Nightly at the moment.
  if (!AppConstants.NIGHTLY_BUILD) {
    ok(true);
    return;
  }

  // start with the pref off.
  await pushPref(EAGER_EVALUATION_PREF, false);
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Check that the setting is disabled");
  checkConsoleSettingState(
    hud,
    ".webconsole-console-settings-menu-item-eager-evaluation",
    false
  );

  is(
    getEagerEvaluationElement(hud),
    null,
    "There's no eager evaluation element"
  );

  // Wait for the autocomplete popup to be displayed so we know the eager evaluation could
  // have occured.
  await setInputValueForAutocompletion(hud, "x + y");
  ok(true, "Eager evaluation is disabled");

  info("Turn on the eager evaluation");
  toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-eager-evaluation"
  );
  await waitFor(() => getEagerEvaluationElement(hud));
  ok(true, "The eager evaluation element is now displayed");
  is(
    Services.prefs.getBoolPref(EAGER_EVALUATION_PREF),
    true,
    "Pref was changed"
  );

  setInputValue(hud, "1 + 2");
  await waitForEagerEvaluationResult(hud, "3");
  ok(true, "Eager evaluation result is displayed");

  info("Turn off the eager evaluation");
  toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-eager-evaluation"
  );
  await waitFor(() => !getEagerEvaluationElement(hud));
  is(
    Services.prefs.getBoolPref(EAGER_EVALUATION_PREF),
    false,
    "Pref was changed"
  );
  ok(true, "Eager evaluation element is no longer displayed");
});
