/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the editor toolbar works as expected when in editor mode.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><p>Test editor toolbar";

add_task(async function () {
  await pushPref("devtools.webconsole.input.editor", false);

  const tab = await addTab(TEST_URI);
  let hud = await openConsole(tab);

  info("Test that the toolbar is not displayed when in editor mode");
  let toolbar = getEditorToolbar(hud);
  is(toolbar, null, "The toolbar isn't displayed when not in editor mode");
  await closeToolbox();

  await pushPref("devtools.webconsole.input.editor", true);
  hud = await openConsole(tab);

  info("Test that the toolbar is displayed when in editor mode");
  toolbar = getEditorToolbar(hud);
  ok(toolbar, "The toolbar is displayed when in editor mode");

  info("Test that the toolbar has the expected items");
  const runButton = toolbar.querySelector(
    ".webconsole-editor-toolbar-executeButton"
  );
  is(runButton.textContent, "Run", "The button has the expected text");
  const keyShortcut =
    (Services.appinfo.OS === "Darwin" ? "Cmd" : "Ctrl") + " + Enter";
  is(
    runButton.getAttribute("title"),
    `Run expression (${keyShortcut}). This won’t clear the input.`,
    "The Run Button has the correct title"
  );

  info("Test that clicking on the Run button works as expected");

  const jsTestStatememts = Object.entries({
    // input: output,
    "`${1 + 1} = 2`": "2 = 2",
    '`${"area" + 51} = aliens?`': "area51 = aliens?",
  });

  for (const [input, output] of jsTestStatememts) {
    // Setting the input value.
    setInputValue(hud, input);
    runButton.click();
    await waitFor(() => findMessageByType(hud, input, ".command"));
    await waitFor(() => findEvaluationResultMessage(hud, output));
    ok(true, "The expression and its result are displayed in the output");
    ok(
      isInputFocused(hud),
      "input is still focused after clicking the Run button"
    );
  }
  // Clear JS Term beform testing history buttons
  setInputValue(hud, "");

  info("Test that clicking the previous expression button works as expected");
  const prevHistoryButton = toolbar.querySelector(
    ".webconsole-editor-toolbar-history-prevExpressionButton"
  );
  is(
    prevHistoryButton.getAttribute("title"),
    "Previous Expression",
    "The Previous Expression Button has the correct title"
  );
  for (const [input] of jsTestStatememts.slice().reverse()) {
    prevHistoryButton.click();
    is(
      getInputValue(hud),
      input,
      `The JS Terminal Editor has the correct previous expresion ${input}`
    );
  }

  info("Test that clicking the next expression button works as expected");
  const nextHistoryButton = toolbar.querySelector(
    ".webconsole-editor-toolbar-history-nextExpressionButton"
  );
  is(
    nextHistoryButton.getAttribute("title"),
    "Next Expression",
    "The Next Expression Button has the correct title"
  );
  nextHistoryButton.click();
  const [nextHistoryJsStatement] = jsTestStatememts.slice(-1).pop();
  is(
    getInputValue(hud),
    nextHistoryJsStatement,
    `The JS Terminal Editor has the correct next expresion ${nextHistoryJsStatement}`
  );
  nextHistoryButton.click();
  is(getInputValue(hud), "");

  info("Test that clicking the pretty print button works as expected");
  const expressionToPrettyPrint = [
    // [raw, prettified, prettifiedWithTab, prettifiedWith4Spaces]
    ["fn=n=>n*n", "fn = n => n * n", "fn = n => n * n", "fn = n => n * n"],
    [
      "{x:1, y:2}",
      "{\n  x: 1,\n  y: 2\n}",
      "{\n\tx: 1,\n\ty: 2\n}",
      "{\n    x: 1,\n    y: 2\n}",
    ],
    [
      "async function test() {await new Promise(res => {})}",
      "async function test() {\n  await new Promise(res => {})\n}",
      "async function test() {\n\tawait new Promise(res => {})\n}",
      "async function test() {\n    await new Promise(res => {})\n}",
    ],
  ];

  const prettyPrintButton = toolbar.querySelector(
    ".webconsole-editor-toolbar-prettyPrintButton"
  );
  ok(prettyPrintButton, "The pretty print button is displayed in editor mode");
  for (const [
    input,
    output,
    outputWithTab,
    outputWith4Spaces,
  ] of expressionToPrettyPrint) {
    // Setting the input value.
    setInputValue(hud, input);
    await pushPref("devtools.editor.tabsize", 2);
    prettyPrintButton.click();
    is(
      getInputValue(hud),
      output,
      `Pretty print works for expression ${input}`
    );
    // Turn on indent with tab.
    await pushPref("devtools.editor.expandtab", false);
    prettyPrintButton.click();
    is(
      getInputValue(hud),
      outputWithTab,
      `Pretty print works for expression ${input} when expandtab is false`
    );
    await pushPref("devtools.editor.expandtab", true);
    // Set indent size to 4.
    await pushPref("devtools.editor.tabsize", 4);
    prettyPrintButton.click();
    is(
      getInputValue(hud),
      outputWith4Spaces,
      `Pretty print works for expression ${input} when tabsize is 4`
    );
    await pushPref("devtools.editor.tabsize", 2);
    ok(
      isInputFocused(hud),
      "input is still focused after clicking the pretty print button"
    );
  }

  info("Test that clicking the close button works as expected");
  const closeButton = toolbar.querySelector(
    ".webconsole-editor-toolbar-closeButton"
  );
  const closeKeyShortcut =
    (Services.appinfo.OS === "Darwin" ? "Cmd" : "Ctrl") + " + B";
  is(
    closeButton.title,
    `Switch back to inline mode (${closeKeyShortcut})`,
    "Close button has expected title"
  );
  closeButton.click();
  await waitFor(() => !isEditorModeEnabled(hud));
  ok(true, "Editor mode is disabled when clicking on the close button");
});

function getEditorToolbar(hud) {
  return hud.ui.outputNode.querySelector(".webconsole-editor-toolbar");
}
