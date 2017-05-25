/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the links from the rule-view to the styleeditor

const STYLESHEET_URL = "data:text/css," + encodeURIComponent(
  ["#first {",
   "color: blue",
   "}"].join("\n"));

const EXTERNAL_STYLESHEET_FILE_NAME = "doc_style_editor_link.css";
const EXTERNAL_STYLESHEET_URL = URL_ROOT + EXTERNAL_STYLESHEET_FILE_NAME;

const DOCUMENT_URL = "data:text/html;charset=utf-8," + encodeURIComponent(`
  <html>
  <head>
  <title>Rule view style editor link test</title>
  <style type="text/css">
  html { color: #000000; }
  div { font-variant: small-caps; color: #000000; }
  .nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em;
  font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">
  </style>
  <style>
  div { font-weight: bold; }
  </style>
  <link rel="stylesheet" type="text/css" href="${STYLESHEET_URL}">
  <link rel="stylesheet" type="text/css" href="${EXTERNAL_STYLESHEET_URL}">
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
  </html>
`);

add_task(function* () {
  yield addTab(DOCUMENT_URL);
  let {toolbox, inspector, view, testActor} = yield openRuleView();
  yield selectNode("div", inspector);

  yield testInlineStyle(view);
  yield testFirstInlineStyleSheet(view, toolbox, testActor);
  yield testSecondInlineStyleSheet(view, toolbox, testActor);
  yield testExternalStyleSheet(view, toolbox, testActor);
  yield testDisabledStyleEditor(view, toolbox);
});

function* testInlineStyle(view) {
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

function* testFirstInlineStyleSheet(view, toolbox, testActor) {
  info("Testing inline stylesheet");

  info("Listening for toolbox switch to the styleeditor");
  let onSwitch = waitForStyleEditor(toolbox);

  info("Clicking an inline stylesheet");
  clickLinkByIndex(view, 4);
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
  testRuleViewLinkLabel(view);
  clickLinkByIndex(view, 3);
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
  testRuleViewLinkLabel(view);
  clickLinkByIndex(view, 1);
  let editor = yield onSelected;

  is(toolbox.currentToolId, "styleeditor",
    "The style editor is selected again");
  yield validateStyleEditorSheet(editor, 2, testActor);
}

function* validateStyleEditorSheet(editor, expectedSheetIndex, testActor) {
  info("validating style editor stylesheet");
  is(editor.styleSheet.styleSheetIndex, expectedSheetIndex,
     "loaded stylesheet index matches document stylesheet");

  let href = editor.styleSheet.href || editor.styleSheet.nodeHref;

  let expectedHref = yield testActor.eval(
    `content.document.styleSheets[${expectedSheetIndex}].href ||
     content.document.location.href`);

  is(href, expectedHref, "loaded stylesheet href matches document stylesheet");
}

function* testDisabledStyleEditor(view, toolbox) {
  info("Testing with the style editor disabled");

  info("Switching to the inspector panel in the toolbox");
  yield toolbox.selectTool("inspector");

  info("Disabling the style editor");
  Services.prefs.setBoolPref("devtools.styleeditor.enabled", false);
  gDevTools.emit("tool-unregistered", "styleeditor");

  info("Clicking on a link");
  testUnselectableRuleViewLink(view, 1);
  clickLinkByIndex(view, 1);

  is(toolbox.currentToolId, "inspector", "The click should have no effect");

  info("Enabling the style editor");
  Services.prefs.setBoolPref("devtools.styleeditor.enabled", true);
  gDevTools.emit("tool-registered", "styleeditor");

  info("Clicking on a link");
  let onStyleEditorSelected = toolbox.once("styleeditor-selected");
  clickLinkByIndex(view, 1);
  yield onStyleEditorSelected;
  is(toolbox.currentToolId, "styleeditor", "Style Editor should be selected");

  Services.prefs.clearUserPref("devtools.styleeditor.enabled");
}

function testRuleViewLinkLabel(view) {
  let link = getRuleViewLinkByIndex(view, 2);
  let labelElem = link.querySelector(".ruleview-rule-source-label");
  let value = labelElem.textContent;
  let tooltipText = labelElem.getAttribute("title");

  is(value, EXTERNAL_STYLESHEET_FILE_NAME + ":1",
    "rule view stylesheet display value matches filename and line number");
  is(tooltipText, EXTERNAL_STYLESHEET_URL + ":1",
    "rule view stylesheet tooltip text matches the full URI path");
}

function testUnselectableRuleViewLink(view, index) {
  let link = getRuleViewLinkByIndex(view, index);
  let unselectable = link.hasAttribute("unselectable");

  ok(unselectable, "Rule view is unselectable");
}

function clickLinkByIndex(view, index) {
  let link = getRuleViewLinkByIndex(view, index);
  link.scrollIntoView();
  link.click();
}
