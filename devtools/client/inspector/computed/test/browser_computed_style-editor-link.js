/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Unknown sheet source");

// Tests the links from the computed view to the style editor.

const STYLESHEET_URL = "data:text/css," + encodeURIComponent(
  ".highlight {color: blue}");

const DOCUMENT_URL = "data:text/html;charset=utf-8," + encodeURIComponent(
  `<html>
   <head>
   <title>Computed view style editor link test</title>
   <style type="text/css">
   html { color: #000000; }
   span { font-variant: small-caps; color: #000000; }
   .nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em;
   font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">
   </style>
   <style>
   div { color: #f06; }
   </style>
   <link rel="stylesheet" type="text/css" href="${STYLESHEET_URL}">
   </head>
   <body>
   <h1>Some header text</h1>
   <p id="salutation" style="font-size: 12pt">hi.</p>
   <p id="body" style="font-size: 12pt">I am a test-case. This text exists
   solely to provide some things to
   <span style="color: yellow" class="highlight">
   highlight</span> and <span style="font-weight: bold">count</span>
   style list-items in the box at right. If you are reading this,
   you should go do something else instead. Maybe read a book. Or better
   yet, write some test-cases for another bit of code.
   <span style="font-style: italic">some text</span></p>
   <p id="closing">more text</p>
   <p>even more text</p>
   </div>
   </body>
   </html>`);

add_task(function* () {
  yield addTab(DOCUMENT_URL);
  let {toolbox, inspector, view, testActor} = yield openComputedView();
  yield selectNode("span", inspector);

  yield testInlineStyle(view);
  yield testFirstInlineStyleSheet(view, toolbox, testActor);
  yield testSecondInlineStyleSheet(view, toolbox, testActor);
  yield testExternalStyleSheet(view, toolbox, testActor);
});

function* testInlineStyle(view) {
  info("Testing inline style");

  yield expandComputedViewPropertyByIndex(view, 0);

  let onTab = waitForTab();
  info("Clicking on the first rule-link in the computed-view");
  clickLinkByIndex(view, 0);

  let tab = yield onTab;

  let tabURI = tab.linkedBrowser.documentURI.spec;
  ok(tabURI.startsWith("view-source:"), "View source tab is open");
  info("Closing tab");
  gBrowser.removeTab(tab);
}

function* testFirstInlineStyleSheet(view, toolbox, testActor) {
  info("Testing inline stylesheet");

  info("Listening for toolbox switch to the styleeditor");
  let onSwitch = waitForStyleEditor(toolbox);

  info("Clicking an inline stylesheet");
  clickLinkByIndex(view, 2);
  let editor = yield onSwitch;

  ok(true, "Switched to the style-editor panel in the toolbox");

  yield validateStyleEditorSheet(editor, 0, testActor);
}

function* testSecondInlineStyleSheet(view, toolbox, testActor) {
  info("Testing second inline stylesheet");

  info("Waiting for the stylesheet editor to be selected");
  let panel = toolbox.getCurrentPanel();
  let onSelected = panel.UI.once("editor-selected");

  info("Switching back to the inspector panel in the toolbox");
  yield toolbox.selectTool("inspector");

  info("Clicking on second inline stylesheet link");
  clickLinkByIndex(view, 4);
  let editor = yield onSelected;

  is(toolbox.currentToolId, "styleeditor",
    "The style editor is selected again");
  yield validateStyleEditorSheet(editor, 1, testActor);
}

function* testExternalStyleSheet(view, toolbox, testActor) {
  info("Testing external stylesheet");

  info("Waiting for the stylesheet editor to be selected");
  let panel = toolbox.getCurrentPanel();
  let onSelected = panel.UI.once("editor-selected");

  info("Switching back to the inspector panel in the toolbox");
  yield toolbox.selectTool("inspector");

  info("Clicking on an external stylesheet link");
  clickLinkByIndex(view, 1);
  let editor = yield onSelected;

  is(toolbox.currentToolId, "styleeditor",
    "The style editor is selected again");
  yield validateStyleEditorSheet(editor, 2, testActor);
}

function* validateStyleEditorSheet(editor, expectedSheetIndex, testActor) {
  info("Validating style editor stylesheet");
  let expectedHref = yield testActor.eval(`
    document.styleSheets[${expectedSheetIndex}].href;
  `);
  is(editor.styleSheet.href, expectedHref,
    "loaded stylesheet matches document stylesheet");
}

function clickLinkByIndex(view, index) {
  let link = getComputedViewLinkByIndex(view, index);
  link.scrollIntoView();
  link.click();
}
