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

// Basic testing of eager evaluation functionality. Expressions which can be
// eagerly evaluated should show their results, and expressions with side
// effects should not perform those side effects.
add_task(async function() {
  await pushPref("devtools.webconsole.input.eagerEvaluation", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  setInputValue(hud, "x + y");
  await waitForEagerEvaluationResult(hud, "7");

  setInputValue(hud, "x + y + undefined");
  await waitForEagerEvaluationResult(hud, "NaN");

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
});

// Test that the currently selected autocomplete result is eagerly evaluated.
add_task(async function() {
  await pushPref("devtools.webconsole.input.eagerEvaluation", true);
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
