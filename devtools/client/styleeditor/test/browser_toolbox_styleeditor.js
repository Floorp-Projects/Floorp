/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that stylesheets from parent and content processes are displayed in the styleeditor.

"use strict";

requestLongerTimeout(4);

const TEST_URI = `data:text/html,<!DOCTYPE html>
  <head>
    <meta charset=utf8>
    <link rel="stylesheet" type="text/css" href="${TEST_BASE_HTTP}simple.css">
  <head>
  <body>Test browser toolbox</body>`;

/* global gToolbox */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

add_task(async function () {
  await pushPref("devtools.browsertoolbox.scope", "everything");
  await pushPref("devtools.styleeditor.transitions", false);
  await addTab(TEST_URI);
  const ToolboxTask = await initBrowserToolboxTask();

  await ToolboxTask.importFunctions({
    waitUntil,
  });

  await ToolboxTask.spawn(null, async () => {
    await gToolbox.selectTool("styleeditor");
    const panel = gToolbox.getCurrentPanel();

    function getStyleEditorItems() {
      return Array.from(
        panel.panelWindow.document.querySelectorAll(".splitview-nav li")
      );
    }

    info(`check if "parent process" stylesheets are displayed`);
    const isUAStyleSheet = el =>
      el.querySelector(".stylesheet-name label").value == "ua.css";
    await waitUntil(() => getStyleEditorItems().find(isUAStyleSheet));
    ok(true, "Found ua.css stylesheet");

    info("check if content page stylesheets are displayed");
    const isTabStyleSheet = el =>
      el.querySelector(".stylesheet-name label").value == "simple.css";
    await waitUntil(() => getStyleEditorItems().find(isTabStyleSheet));
    ok(true, "Found simple.css tab stylesheet");

    info("Select the stylesheet and update its content");
    const contentStylesheetSummaryEl =
      getStyleEditorItems().find(isTabStyleSheet);

    let tabStyleSheetEditor;
    if (panel.UI.selectedEditor.friendlyName === "simple.css") {
      // simple.css might be selected by default, depending on the order in
      // which the stylesheets have been loaded in the style editor.
      tabStyleSheetEditor = panel.UI.selectedEditor;
    } else {
      // We might get events for the initial, default selected stylesheet, so wait until
      // we get the one for the simple.css stylesheet.
      const onTabStyleSheetEditorSelected = new Promise(resolve => {
        const onEditorSelected = editor => {
          if (editor.summary == contentStylesheetSummaryEl) {
            resolve(editor);
            panel.UI.off("editor-selected", onEditorSelected);
          }
        };
        panel.UI.on("editor-selected", onEditorSelected);
      });
      panel.UI.setActiveSummary(contentStylesheetSummaryEl);
      tabStyleSheetEditor = await onTabStyleSheetEditorSelected;
    }

    info("Wait for sourceEditor to be available");
    await waitUntil(() => tabStyleSheetEditor.sourceEditor);

    const onStyleApplied = tabStyleSheetEditor.once("style-applied");
    tabStyleSheetEditor.sourceEditor.setText(
      tabStyleSheetEditor.sourceEditor.getText() + "\n body {color: red;}"
    );
    await onStyleApplied;
  });

  info("Check that the edit done in the style editor were applied to the page");
  const bodyColorStyle = await getComputedStyleProperty({
    selector: "body",
    name: "color",
  });

  is(
    bodyColorStyle,
    "rgb(255, 0, 0)",
    "Changes made to simple.css were applied to the page"
  );

  await ToolboxTask.destroy();
});
