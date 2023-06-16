/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>Test that eager evaluation can't log warnings in the output`;
add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  setInputValue(hud, `document.getElementById("")`);
  await waitForEagerEvaluationResult(hud, "null");

  info("Wait for a bit so a warning message could be displayed");
  await wait(2000);
  is(
    findWarningMessage(hud, "getElementById"),
    undefined,
    "The eager evaluation did not triggered a warning message"
  );

  info("Sanity check for the warning message when the expression is evaluated");
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => findWarningMessage(hud, "getElementById"));
  ok(true, "Evaluation of the expression does trigger the warning message");
});
