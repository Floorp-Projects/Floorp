/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that style editor loads correctly.

const TESTCASE_URI = TEST_BASE_HTTP + "longload.html";

add_task(function* () {
  // launch Style Editor right when the tab is created (before load)
  // this checks that the Style Editor still launches correctly when it is
  // opened *while* the page is still loading. The Style Editor should not
  // signal that it is loaded until the accompanying content page is loaded.
  let tabAdded = addTab(TESTCASE_URI);
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let styleEditorLoaded = gDevTools.showToolbox(target, "styleeditor");

  yield Promise.all([tabAdded, styleEditorLoaded]);

  let toolbox = gDevTools.getToolbox(target);
  let panel = toolbox.getPanel("styleeditor");
  let { panelWindow } = panel;

  let root = panelWindow.document.querySelector(".splitview-root");
  ok(!root.classList.contains("loading"),
     "style editor root element does not have 'loading' class name anymore");

  let button = panelWindow.document.querySelector(".style-editor-newButton");
  ok(!button.hasAttribute("disabled"),
     "new style sheet button is enabled");

  button = panelWindow.document.querySelector(".style-editor-importButton");
  ok(!button.hasAttribute("disabled"),
     "import button is enabled");
});
