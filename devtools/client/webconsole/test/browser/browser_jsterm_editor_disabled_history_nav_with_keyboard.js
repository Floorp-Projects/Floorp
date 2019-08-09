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

  const testExpressions = [
    "`Mozilla 😍 Firefox`",
    "`Firefox Devtools are awesome`",
    "`2 + 2 = 5?`",
    "`I'm running out of ideas...`",
    "`🌑 🌒 🌓 🌔 🌕 🌖 🌗 🌘`",
    "`🌪🌪 🐄 🐄 🏠 🐄 🐄 ⛈`",
    "`🌈 🌈 🌈 🦄 🦄 🌈 🌈 🌈`",
    "`Time to perform the test 🤪`",
  ];

  info("Executing a bunch of non-sense JS expression");
  for (const expression of testExpressions) {
    // Wait until we get the result of the command.
    await executeAndWaitForMessage(hud, expression, "", ".result");
    ok(true, `JS expression executed successfully: ${expression} `);
  }

  info("Test that pressing ArrowUp does nothing");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(getInputValue(hud), "", "Good! There is no text in the JS Editor");

  info("Test that pressing multiple times ArrowUp does nothing");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(getInputValue(hud), "", "Good! Again, there is no text in the JS Editor");

  info(
    "Move somewhere in the middle of the history using the navigation buttons and test again"
  );
  const prevHistoryButton = getEditorToolbar(hud).querySelector(
    ".webconsole-editor-toolbar-history-prevExpressionButton"
  );
  info("Pressing 3 times the previous history button");
  prevHistoryButton.click();
  prevHistoryButton.click();
  prevHistoryButton.click();
  const jsExpression = testExpressions[testExpressions.length - 3];
  is(
    getInputValue(hud),
    jsExpression,
    "Sweet! We are in the right position of the history"
  );

  info("Test again that pressing ArrowUp does nothing");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    getInputValue(hud),
    jsExpression,
    "OMG! We have some cows in the JS Editor!"
  );

  info("Test again that pressing multiple times ArrowUp does nothing");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    getInputValue(hud),
    jsExpression,
    "Awesome! The cows are still there in the JS Editor!"
  );

  info("Test that pressing ArrowDown does nothing");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    getInputValue(hud),
    jsExpression,
    "Super! We still have the cows in the JS Editor!"
  );

  info("Test that pressing multiple times ArrowDown does nothing");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(getInputValue(hud), jsExpression, "And the cows are still there...");
});

function getEditorToolbar(hud) {
  return hud.ui.outputNode.querySelector(".webconsole-editor-toolbar");
}
