/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that style editor loads correctly.

const TESTCASE_URI = TEST_BASE_HTTPS + "longload.html";

add_task(async function () {
  // launch Style Editor right when the tab is created (before load)
  // this checks that the Style Editor still launches correctly when it is
  // opened *while* the page is still loading. The Style Editor should not
  // signal that it is loaded until the accompanying content page is loaded.
  const tabAdded = addTab(TESTCASE_URI);
  const tab = gBrowser.selectedTab;
  const styleEditorLoaded = gDevTools.showToolboxForTab(tab, {
    toolId: "styleeditor",
  });

  await Promise.all([tabAdded, styleEditorLoaded]);

  const toolbox = gDevTools.getToolboxForTab(tab);
  const panel = toolbox.getPanel("styleeditor");
  const { panelWindow } = panel;

  ok(
    !getRootElement(panel).classList.contains("loading"),
    "style editor root element does not have 'loading' class name anymore"
  );

  let button = panelWindow.document.querySelector(".style-editor-newButton");
  ok(!button.hasAttribute("disabled"), "new style sheet button is enabled");

  button = panelWindow.document.querySelector(".style-editor-importButton");
  ok(!button.hasAttribute("disabled"), "import button is enabled");
});
