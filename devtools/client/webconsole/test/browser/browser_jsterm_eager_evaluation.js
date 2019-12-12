/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
<script>
let x = 3, y = 4;
function foo() {
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

  setInputValue(hud, "foo()");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + 2");
  await waitForEagerEvaluationResult(hud, "5");

  setInputValue(hud, "x +");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + z");
  await waitForEagerEvaluationResult(hud, /ReferenceError/);
});
