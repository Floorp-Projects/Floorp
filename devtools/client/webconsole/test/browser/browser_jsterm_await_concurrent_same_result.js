/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that hitting Ctrl + E does toggle the editor mode.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1519105

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Test concurrent top-level await expressions returning same value";

add_task(async function() {
  // Enable editor mode as we'll be able to quicly trigger multiple evaluations.
  await pushPref("devtools.webconsole.features.editor", true);
  await pushPref("devtools.webconsole.input.editor", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  setInputValue(
    hud,
    `await new Promise(res => setTimeout(() => res("foo"), 5000))`
  );

  info("Evaluate the expression 3 times in a row");
  const executeButton = hud.ui.outputNode.querySelector(
    ".webconsole-editor-toolbar-executeButton"
  );

  executeButton.click();
  executeButton.click();
  executeButton.click();

  await waitFor(
    () => findMessages(hud, "foo", ".result").length === 3,
    "Waiting for all results to be printed in console",
    1000
  );
  ok(true, "There are as many results as commands");

  Services.prefs.clearUserPref("devtools.webconsole.features.editor");
  Services.prefs.clearUserPref("devtools.webconsole.input.editor");
});
