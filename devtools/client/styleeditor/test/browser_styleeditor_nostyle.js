/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that 'no styles' indicator is shown if a page doesn't contain any style
// sheets.

const TESTCASE_URI = TEST_BASE_HTTP + "nostyle.html";

add_task(function* () {
  let { panel } = yield openStyleEditorForURL(TESTCASE_URI);
  let { panelWindow } = panel;

  let root = panelWindow.document.querySelector(".splitview-root");
  ok(!root.classList.contains("loading"),
     "style editor root element does not have 'loading' class name anymore");

  ok(root.querySelector(".empty.placeholder"), "showing 'no style' indicator");

  let button = panelWindow.document.querySelector(".style-editor-newButton");
  ok(!button.hasAttribute("disabled"),
     "new style sheet button is enabled");

  button = panelWindow.document.querySelector(".style-editor-importButton");
  ok(!button.hasAttribute("disabled"),
     "import button is enabled");
});
