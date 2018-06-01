/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cached autocomplete results are used when the new
// user input is a subset of the existing completion results.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test cached autocompletion results";

add_task(async function() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  const {
    autocompletePopup: popup,
    completeNode,
    inputNode: input,
  } = jsterm;

  const jstermComplete = (value, offset) =>
    jstermSetValueAndComplete(jsterm, value, offset);

  // Test if 'doc' gives 'document'
  await jstermComplete("doc");
  is(input.value, "doc", "'docu' completion (input.value)");
  is(completeNode.value, "   ument", "'docu' completion (completeNode)");

  // Test typing 'window.'.
  await jstermComplete("window.");
  ok(popup.getItems().length > 0, "'window.' gave a list of suggestions");

  await jsterm.execute("window.docfoobar = true");

  // Test typing 'window.doc'.
  await jstermComplete("window.doc");
  ok(!getPopupLabels(popup).includes("docfoobar"),
    "autocomplete cached results do not contain docfoobar. list has not been updated");

  // Test that backspace does not cause a request to the server
  await jstermComplete("window.do");
  ok(!getPopupLabels(popup).includes("docfoobar"),
    "autocomplete cached results do not contain docfoobar. list has not been updated");

  await jsterm.execute("delete window.docfoobar");

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

  await jsterm.execute("window.docfoobar = true");

  // Make sure 'dump(window.doc)' does not contain 'docfoobar'.
  await jstermComplete("dump(window.doc)", -1);
  ok(!getPopupLabels(popup).includes("docfoobar"),
    "autocomplete cached results do not contain docfoobar. list has not been updated");
});

function getPopupLabels(popup) {
  return popup.getItems().map(item => item.label);
}
