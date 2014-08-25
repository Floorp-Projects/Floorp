/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the links from the computed view to the style editor

const STYLESHEET_URL = "data:text/css,"+encodeURIComponent(
  [".highlight {",
   "color: blue",
   "}"].join("\n"));

const DOCUMENT_URL = "data:text/html;charset=utf-8,"+encodeURIComponent(
  ['<html>' +
   '<head>' +
   '<title>Computed view style editor link test</title>',
   '<style type="text/css"> ',
   'html { color: #000000; } ',
   'span { font-variant: small-caps; color: #000000; } ',
   '.nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em; ',
   'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">',
   '</style>',
   '<style>',
   'div { color: #f06; }',
   '</style>',
   '<link rel="stylesheet" type="text/css" href="'+STYLESHEET_URL+'">',
   '</head>',
   '<body>',
   '<h1>Some header text</h1>',
   '<p id="salutation" style="font-size: 12pt">hi.</p>',
   '<p id="body" style="font-size: 12pt">I am a test-case. This text exists ',
   'solely to provide some things to ',
   '<span style="color: yellow" class="highlight">',
   'highlight</span> and <span style="font-weight: bold">count</span> ',
   'style list-items in the box at right. If you are reading this, ',
   'you should go do something else instead. Maybe read a book. Or better ',
   'yet, write some test-cases for another bit of code. ',
   '<span style="font-style: italic">some text</span></p>',
   '<p id="closing">more text</p>',
   '<p>even more text</p>',
   '</div>',
   '</body>',
   '</html>'].join("\n"));

let test = asyncTest(function*() {
  yield addTab(DOCUMENT_URL);

  info("Opening the computed-view");
  let {toolbox, inspector, view} = yield openComputedView();

  info("Selecting the test node");
  yield selectNode("span", inspector);

  yield testInlineStyle(view, inspector);
  yield testFirstInlineStyleSheet(view, toolbox);
  yield testSecondInlineStyleSheet(view, toolbox);
  yield testExternalStyleSheet(view, toolbox);
});

function* testInlineStyle(view, inspector) {
  info("Testing inline style");

  yield expandComputedViewPropertyByIndex(view, inspector, 0);

  let onWindow = waitForWindow();
  info("Clicking on the first rule-link in the computed-view");
  clickLinkByIndex(view, 0);

  let win = yield onWindow;

  let windowType = win.document.documentElement.getAttribute("windowtype");
  is(windowType, "navigator:view-source", "View source window is open");
  info("Closing window");
  win.close();
}

function* testFirstInlineStyleSheet(view, toolbox) {
  info("Testing inline stylesheet");

  info("Listening for toolbox switch to the styleeditor");
  let onSwitch = waitForStyleEditor(toolbox);

  info("Clicking an inline stylesheet");
  clickLinkByIndex(view, 2);
  let editor = yield onSwitch;

  ok(true, "Switched to the style-editor panel in the toolbox");

  validateStyleEditorSheet(editor, 0);
}

function* testSecondInlineStyleSheet(view, toolbox) {
  info("Testing second inline stylesheet");

  info("Waiting for the stylesheet editor to be selected");
  let panel = toolbox.getCurrentPanel();
  let onSelected = panel.UI.once("editor-selected");

  info("Switching back to the inspector panel in the toolbox");
  yield toolbox.selectTool("inspector");

  info("Clicking on second inline stylesheet link");
  clickLinkByIndex(view, 4);
  let editor = yield onSelected;

  is(toolbox.currentToolId, "styleeditor", "The style editor is selected again");
  validateStyleEditorSheet(editor, 1);
}

function* testExternalStyleSheet(view, toolbox) {
  info("Testing external stylesheet");

  info("Waiting for the stylesheet editor to be selected");
  let panel = toolbox.getCurrentPanel();
  let onSelected = panel.UI.once("editor-selected");

  info("Switching back to the inspector panel in the toolbox");
  yield toolbox.selectTool("inspector");

  info("Clicking on an external stylesheet link");
  clickLinkByIndex(view, 1);
  let editor = yield onSelected;

  is(toolbox.currentToolId, "styleeditor", "The style editor is selected again");
  validateStyleEditorSheet(editor, 2);
}

function validateStyleEditorSheet(editor, expectedSheetIndex) {
  info("Validating style editor stylesheet");
  let sheet = content.document.styleSheets[expectedSheetIndex];
  is(editor.styleSheet.href, sheet.href, "loaded stylesheet matches document stylesheet");
}

function clickLinkByIndex(view, index) {
  let link = getComputedViewLinkByIndex(view, index);
  link.scrollIntoView();
  link.click();
}
