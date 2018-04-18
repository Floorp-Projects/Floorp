/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that source contents are invalidated when the target navigates.
 */

const TAB_URL = EXAMPLE_URL + "sjs_post-page.sjs";

const FORM = "<form method=\"POST\"><input type=\"submit\"></form>";
const GET_CONTENT = "<script>\"GET\";</script>" + FORM;
const POST_CONTENT = "<script>\"POST\";</script>" + FORM;

add_task(async function() {
  // Disable rcwn to make cache behavior deterministic.
  await pushPref("network.http.rcwn.enabled", false);

  let options = {
    source: TAB_URL,
    line: 1
  };
  let [tab,, panel] = await initDebugger(TAB_URL, options);
  let win = panel.panelWin;
  let editor = win.DebuggerView.editor;
  let queries = win.require("./content/queries");
  let getState = win.DebuggerController.getState;

  let source = queries.getSelectedSource(getState());

  is(queries.getSourceCount(getState()), 1,
    "There should be one source displayed in the view.");
  is(source.url, TAB_URL,
    "The correct source is currently selected in the view.");
  is(editor.getText(), GET_CONTENT,
    "The currently shown source contains bacon. Mmm, delicious!");

  // Submit the form and wait for debugger update
  let onSourceUpdated = waitForSourceShown(panel, TAB_URL);
  await ContentTask.spawn(tab.linkedBrowser, null, function () {
    content.document.querySelector("input[type=\"submit\"]").click();
  });
  await onSourceUpdated;

  // Verify that the source updates to the POST page content
  source = queries.getSelectedSource(getState());
  is(queries.getSourceCount(getState()), 1,
    "There should be one source displayed in the view.");
  is(source.url, TAB_URL,
    "The correct source is currently selected in the view.");
  is(editor.getText(), POST_CONTENT,
    "The currently shown source contains bacon. Mmm, delicious!");

  await closeDebuggerAndFinish(panel);
});
