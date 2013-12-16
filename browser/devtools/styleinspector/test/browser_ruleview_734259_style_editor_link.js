/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let win;
let doc;
let contentWindow;
let inspector;
let toolbox;

let tempScope = {};
Cu.import("resource://gre/modules/Services.jsm", tempScope);
let Services = tempScope.Services;

const STYLESHEET_URL = "data:text/css,"+encodeURIComponent(
  ["#first {",
   "color: blue",
   "}"].join("\n"));

const DOCUMENT_URL = "data:text/html,"+encodeURIComponent(
  ['<html>' +
   '<head>' +
   '<title>Rule view style editor link test</title>',
   '<style type="text/css"> ',
   'html { color: #000000; } ',
   'div { font-variant: small-caps; color: #000000; } ',
   '.nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em; ',
   'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">',
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

function openToolbox() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(aToolbox) {
    toolbox = aToolbox;
    inspector = toolbox.getCurrentPanel();
    inspector.sidebar.select("ruleview");
    highlightNode();
  });
}

function highlightNode()
{
  // Highlight a node.
  let div = content.document.getElementsByTagName("div")[0];

  inspector.selection.setNode(div);
  inspector.once("inspector-updated", () => {
    is(inspector.selection.node, div, "selection matches the div element");
    testInlineStyle();
  });
}

function testInlineStyle()
{
  info("clicking an inline style");

  Services.ww.registerNotification(function onWindow(aSubject, aTopic) {
    if (aTopic != "domwindowopened") {
      return;
    }

    win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function windowLoad() {
      win.removeEventListener("load", windowLoad);
      let windowType = win.document.documentElement.getAttribute("windowtype");
      is(windowType, "navigator:view-source", "view source window is open");
      win.close();
      Services.ww.unregisterNotification(onWindow);
      executeSoon(() => {
        testInlineStyleSheet();
      });
    });
  });

  let link = getLinkByIndex(0);
  link.scrollIntoView();
  link.click();
}

function testInlineStyleSheet()
{
  info("clicking an inline stylesheet");

  toolbox.once("styleeditor-ready", function(id, aToolbox) {
    let panel = toolbox.getCurrentPanel();

    panel.UI.once("editor-selected", (event, editor) => {
      validateStyleEditorSheet(editor, 0);
      executeSoon(() => {
        testExternalStyleSheet(toolbox);
      });
    });
  });

  let link = getLinkByIndex(2);
  link.scrollIntoView();
  link.click();
}

function testExternalStyleSheet(toolbox) {
  info ("clicking an external stylesheet");

  let panel = toolbox.getCurrentPanel();
  panel.UI.once("editor-selected", (event, editor) => {
    is(toolbox.currentToolId, "styleeditor", "style editor tool selected");
    validateStyleEditorSheet(editor, 1);
    finishUp();
  });

  toolbox.selectTool("inspector").then(function () {
    let link = getLinkByIndex(1);
    link.scrollIntoView();
    link.click();
  });
}

function validateStyleEditorSheet(aEditor, aExpectedSheetIndex)
{
  info("validating style editor stylesheet");
  let sheet = doc.styleSheets[aExpectedSheetIndex];
  is(aEditor.styleSheet.href, sheet.href, "loaded stylesheet matches document stylesheet");
}

function getLinkByIndex(aIndex)
{
  let contentDoc = ruleView().doc;
  contentWindow = contentDoc.defaultView;
  let links = contentDoc.querySelectorAll(".ruleview-rule-source");
  return links[aIndex];
}

function finishUp()
{
  gBrowser.removeCurrentTab();
  contentWindow = doc = inspector = toolbox = win = null;
  finish();
}

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    doc = content.document;
    waitForFocus(openToolbox, content);
  }, true);

  content.location = DOCUMENT_URL;
}
