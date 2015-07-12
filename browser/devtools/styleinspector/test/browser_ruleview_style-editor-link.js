/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Unknown sheet source");

// Test the links from the rule-view to the styleeditor

const STYLESHEET_URL = "data:text/css,"+encodeURIComponent(
  ["#first {",
   "color: blue",
   "}"].join("\n"));

const EXTERNAL_STYLESHEET_FILE_NAME = "doc_style_editor_link.css";
const EXTERNAL_STYLESHEET_URL = TEST_URL_ROOT + EXTERNAL_STYLESHEET_FILE_NAME;

const DOCUMENT_URL = "data:text/html;charset=utf-8,"+encodeURIComponent(
  ['<html>' +
   '<head>' +
   '<title>Rule view style editor link test</title>',
   '<style type="text/css"> ',
   'html { color: #000000; } ',
   'div { font-variant: small-caps; color: #000000; } ',
   '.nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em; ',
   'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">',
   '</style>',
   '<style>',
   'div { font-weight: bold; }',
   '</style>',
   '<link rel="stylesheet" type="text/css" href="'+STYLESHEET_URL+'">',
   '<link rel="stylesheet" type="text/css" href="'+EXTERNAL_STYLESHEET_URL+'">',
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

add_task(function*() {
  yield addTab(DOCUMENT_URL);
  let {toolbox, inspector, view} = yield openRuleView();

  info("Select the test node");
  yield selectNode("div", inspector);

  yield testInlineStyle(view, inspector);
  yield testFirstInlineStyleSheet(view, toolbox);
  yield testSecondInlineStyleSheet(view, toolbox);
  yield testExternalStyleSheet(view, toolbox);
});

function* testInlineStyle(view, inspector) {
  info("Testing inline style");

  let onTab = waitForTab();
  info("Clicking on the first link in the rule-view");
  clickLinkByIndex(view, 0);

  let tab = yield onTab;

  let tabURI = tab.linkedBrowser.documentURI.spec;
  ok(tabURI.startsWith("view-source:"), "View source tab is open");
  info("Closing tab");
  gBrowser.removeTab(tab);
}

function* testFirstInlineStyleSheet(view, toolbox) {
  info("Testing inline stylesheet");

  info("Listening for toolbox switch to the styleeditor");
  let onSwitch = waitForStyleEditor(toolbox);

  info("Clicking an inline stylesheet");
  clickLinkByIndex(view, 4);
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
  testRuleViewLinkLabel(view);
  clickLinkByIndex(view, 3);
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
  testRuleViewLinkLabel(view);
  clickLinkByIndex(view, 1);
  let editor = yield onSelected;

  is(toolbox.currentToolId, "styleeditor", "The style editor is selected again");
  validateStyleEditorSheet(editor, 2);
}

function validateStyleEditorSheet(editor, expectedSheetIndex) {
  info("validating style editor stylesheet");
  is(editor.styleSheet.styleSheetIndex, expectedSheetIndex,
     "loaded stylesheet index matches document stylesheet");

  let sheet = content.document.styleSheets[expectedSheetIndex];
  is(editor.styleSheet.href, sheet.href, "loaded stylesheet href matches document stylesheet");
}

function testRuleViewLinkLabel(view) {
  let link = getRuleViewLinkByIndex(view, 2);
  let labelElem = link.querySelector(".ruleview-rule-source-label");
  let value = labelElem.getAttribute("value");
  let tooltipText = labelElem.getAttribute("tooltiptext");

  is(value, EXTERNAL_STYLESHEET_FILE_NAME + ":1",
    "rule view stylesheet display value matches filename and line number");
  is(tooltipText, EXTERNAL_STYLESHEET_URL + ":1",
    "rule view stylesheet tooltip text matches the full URI path");
}

function clickLinkByIndex(view, index) {
  let link = getRuleViewLinkByIndex(view, index);
  link.scrollIntoView();
  link.click();
}
