/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the links from the computed view to the style editor.

const STYLESHEET_URL =
  "data:text/css," + encodeURIComponent(".highlight {color: blue}");

const DOCUMENT_URL =
  "data:text/html;charset=utf-8," +
  encodeURIComponent(
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
   </html>`
  );

add_task(async function() {
  await addTab(DOCUMENT_URL);
  const { toolbox, inspector, view } = await openComputedView();
  await selectNode("span", inspector);

  await testInlineStyle(view);
  await testFirstInlineStyleSheet(view, toolbox);
  await testSecondInlineStyleSheet(view, toolbox);
  await testExternalStyleSheet(view, toolbox);
});

async function testInlineStyle(view) {
  info("Testing inline style");

  await expandComputedViewPropertyByIndex(view, 0);

  const onTab = waitForTab();
  info("Clicking on the first rule-link in the computed-view");
  clickLinkByIndex(view, 0);

  const tab = await onTab;

  const tabURI = tab.linkedBrowser.documentURI.spec;
  ok(tabURI.startsWith("view-source:"), "View source tab is open");
  info("Closing tab");
  gBrowser.removeTab(tab);
}

async function testFirstInlineStyleSheet(view, toolbox) {
  info("Testing inline stylesheet");

  info("Listening for toolbox switch to the styleeditor");
  const onSwitch = waitForStyleEditor(toolbox);

  info("Clicking an inline stylesheet");
  clickLinkByIndex(view, 2);
  const editor = await onSwitch;

  ok(true, "Switched to the style-editor panel in the toolbox");

  await validateStyleEditorSheet(editor, 0);
}

async function testSecondInlineStyleSheet(view, toolbox) {
  info("Testing second inline stylesheet");

  info("Waiting for the stylesheet editor to be selected");
  const panel = toolbox.getCurrentPanel();
  const onSelected = panel.UI.once("editor-selected");

  info("Switching back to the inspector panel in the toolbox");
  await toolbox.selectTool("inspector");

  info("Clicking on second inline stylesheet link");
  clickLinkByIndex(view, 4);
  const editor = await onSelected;

  is(
    toolbox.currentToolId,
    "styleeditor",
    "The style editor is selected again"
  );
  await validateStyleEditorSheet(editor, 1);
}

async function testExternalStyleSheet(view, toolbox) {
  info("Testing external stylesheet");

  info("Waiting for the stylesheet editor to be selected");
  const panel = toolbox.getCurrentPanel();
  const onSelected = panel.UI.once("editor-selected");

  info("Switching back to the inspector panel in the toolbox");
  await toolbox.selectTool("inspector");

  info("Clicking on an external stylesheet link");
  clickLinkByIndex(view, 1);
  const editor = await onSelected;

  is(
    toolbox.currentToolId,
    "styleeditor",
    "The style editor is selected again"
  );
  await validateStyleEditorSheet(editor, 2);
}

async function validateStyleEditorSheet(editor, expectedSheetIndex) {
  info("Validating style editor stylesheet");
  const expectedHref = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [expectedSheetIndex],
    _expectedSheetIndex =>
      content.document.styleSheets[_expectedSheetIndex].href
  );
  is(
    editor.styleSheet.href,
    expectedHref,
    "loaded stylesheet matches document stylesheet"
  );
}

function clickLinkByIndex(view, index) {
  const link = getComputedViewLinkByIndex(view, index);
  link.scrollIntoView();
  link.click();
}
