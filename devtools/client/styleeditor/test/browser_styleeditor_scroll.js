/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that editor scrolls to correct line if it's selected with
//  * selectStyleSheet (specified line)
//  * click on the sidebar item (line before the editor was unselected)
// See bug 1148086.

const SIMPLE = TEST_BASE_HTTP + "simple.css";
const LONG = TEST_BASE_HTTP + "doc_long.css";
const DOCUMENT_WITH_LONG_SHEET = "data:text/html;charset=UTF-8," +
        encodeURIComponent(
          ["<!DOCTYPE html>",
           "<html>",
           " <head>",
           "  <title>Editor scroll test page</title>",
           '  <link rel="stylesheet" type="text/css" href="' + SIMPLE + '">',
           '  <link rel="stylesheet" type="text/css" href="' + LONG + '">',
           " </head>",
           " <body>Editor scroll test page</body>",
           "</html>"
          ].join("\n"));
const LINE_TO_SELECT = 201;

add_task(async function() {
  const { ui } = await openStyleEditorForURL(DOCUMENT_WITH_LONG_SHEET);

  is(ui.editors.length, 2, "Two editors present.");

  const simpleEditor = ui.editors[0];
  const longEditor = ui.editors[1];

  info(`Selecting doc_long.css and scrolling to line ${LINE_TO_SELECT}`);

  // We need to wait for editor-selected if we want to check the scroll
  // position as scrolling occurs after selectStyleSheet resolves but before the
  // event is emitted.
  let selectEventPromise = waitForEditorToBeSelected(longEditor, ui);
  await ui.selectStyleSheet(longEditor.styleSheet, LINE_TO_SELECT);
  await selectEventPromise;

  info("Checking that the correct line is visible after initial load");

  const { from, to } = longEditor.sourceEditor.getViewport();
  info(`Lines ${from}-${to} are visible (expected ${LINE_TO_SELECT}).`);

  ok(from <= LINE_TO_SELECT, "The editor scrolled too much.");
  ok(to >= LINE_TO_SELECT, "The editor scrolled too little.");

  const initialScrollTop = longEditor.sourceEditor.getScrollInfo().top;
  info(`Storing scrollTop = ${initialScrollTop} for later comparison.`);

  info("Selecting the first editor (simple.css)");
  await ui.selectStyleSheet(simpleEditor.styleSheet);

  info("Selecting doc_long.css again.");
  selectEventPromise = waitForEditorToBeSelected(longEditor, ui);

  // Can't use ui.selectStyleSheet here as it will scroll the editor back to top
  // and we want to check that the previous scroll position is restored.
  const summary = await ui.getEditorSummary(longEditor);
  ui._view.activeSummary = summary;

  info("Waiting for doc_long.css to be selected.");
  await selectEventPromise;

  const scrollTop = longEditor.sourceEditor.getScrollInfo().top;
  is(scrollTop, initialScrollTop,
    "Scroll top was restored after the sheet was selected again.");
});

/**
 * A helper that waits "editor-selected" event for given editor.
 *
 * @param {StyleSheetEditor} editor
 *        The editor to wait for.
 * @param {StyleEditorUI} ui
 *        The StyleEditorUI the editor belongs to.
 */
var waitForEditorToBeSelected = async function(editor, ui) {
  info(`Waiting for ${editor.friendlyName} to be selected.`);
  let selected = await ui.once("editor-selected");
  while (selected != editor) {
    info(`Ignored editor-selected for editor ${editor.friendlyName}.`);
    selected = await ui.once("editor-selected");
  }

  info(`Got editor-selected for ${editor.friendlyName}.`);
};
