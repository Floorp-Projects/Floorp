/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cached autocomplete results are used when the new
// user input is a subset of the existing completion results.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test cached autocompletion results";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  const { autocompletePopup: popup } = jsterm;

  const jstermComplete = (value, caretPosition) =>
    jstermSetValueAndComplete(jsterm, value, caretPosition);

  // Test if 'doc' gives 'document'
  await jstermComplete("doc");
  is(jsterm.getInputValue(), "doc", "'docu' completion (input.value)");
  checkJsTermCompletionValue(jsterm, "   ument", "'docu' completion (completeNode)");

  // Test typing 'window.'.'
  await jstermComplete("window.");
  ok(popup.getItems().length > 0, "'window.' gave a list of suggestions");

  info("Add a property on the window object");
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.window.docfoobar = true;
  });

  // Test typing d (i.e. input is now 'window.d').
  let onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("d");
  await onUpdated;
  ok(!getPopupLabels(popup).includes("docfoobar"),
    "autocomplete popup does not contain docfoobar. List has not been updated");

  // Test typing o (i.e. input is now 'window.do').
  jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("o");
  await onUpdated;
  ok(!getPopupLabels(popup).includes("docfoobar"),
    "autocomplete popup does not contain docfoobar. List has not been updated");

  // Test that backspace does not cause a request to the server
  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Backspace");
  await onUpdated;
  ok(!getPopupLabels(popup).includes("docfoobar"),
    "autocomplete cached results do not contain docfoobar. list has not been updated");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    delete content.wrappedJSObject.window.docfoobar;
  });

  // Test if 'window.getC' gives 'getComputedStyle'
  await jstermComplete("window.");
  await jstermComplete("window.getC");
  ok(getPopupLabels(popup).includes("getComputedStyle"),
    "autocomplete results do contain getComputedStyle");

  // Test if 'dump(d' gives non-zero results
  await jstermComplete("dump(d");
  ok(popup.getItems().length > 0, "'dump(d' gives non-zero results");

  // Test that 'dump(window.)' works.
  await jstermComplete("dump(window.)", -1);
  ok(popup.getItems().length > 0, "'dump(window.' gave a list of suggestions");

  info("Add a property on the window object");
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.window.docfoobar = true;
  });

  // Make sure 'dump(window.d)' does not contain 'docfoobar'.
  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("d");
  await onUpdated;
  ok(!getPopupLabels(popup).includes("docfoobar"),
    "autocomplete cached results do not contain docfoobar. list has not been updated");
}

function getPopupLabels(popup) {
  return popup.getItems().map(item => item.label);
}
