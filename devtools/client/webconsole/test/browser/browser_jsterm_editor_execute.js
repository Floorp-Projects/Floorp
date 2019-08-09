/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that user input is not cleared when 'devtools.webconsole.input.editor'
// is set to true.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1519313

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Web Console test for bug 1519313";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);
  await pushPref("devtools.webconsole.input.editor", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  const expression = `x = 10`;
  setInputValue(hud, expression);
  await executeAndWaitForMessage(hud, undefined, "", ".result");
  is(getInputValue(hud), expression, "input line is not cleared after submit");
});
