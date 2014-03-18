/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let win;
let doc;
let contentWindow;
let inspector;
let toolbox;

const TESTCASE_URI = TEST_BASE_HTTPS + "sourcemaps.html";
const PREF = "devtools.styleeditor.source-maps-enabled";

const SCSS_LOC = "sourcemaps.scss:4";
const CSS_LOC = "sourcemaps.css:1";

function test()
{
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF, true);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    doc = content.document;
    waitForFocus(openToolbox, content);
  }, true);

  content.location = TESTCASE_URI;
}

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

  inspector.selection.setNode(div, "test");
  inspector.once("inspector-updated", () => {
    is(inspector.selection.node, div, "selection matches the div element");
    testRuleViewLink();
  });
}

function testRuleViewLink() {
  verifyLinkText(SCSS_LOC, testTogglePref);
}

function testTogglePref() {
  Services.prefs.setBoolPref(PREF, false);

  verifyLinkText(CSS_LOC, () => {
    Services.prefs.setBoolPref(PREF, true);

    testClickingLink();
  })
}

function testClickingLink() {
  toolbox.once("styleeditor-ready", function(id, aToolbox) {
    let panel = toolbox.getCurrentPanel();
    panel.UI.on("editor-selected", (event, editor) => {
      // The style editor selects the first sheet at first load before
      // selecting the desired sheet.
      if (editor.styleSheet.href.endsWith("scss")) {
        info("original source editor selected");
        editor.getSourceEditor().then(editorSelected);
      }
    });
  });

  let link = getLinkByIndex(1);

  info("clicking rule view link");
  link.scrollIntoView();
  link.click();
}

function editorSelected(editor) {
  let href = editor.styleSheet.href;
  ok(href.endsWith("sourcemaps.scss"), "selected stylesheet is correct one");

  let {line, col} = editor.sourceEditor.getCursor();
  is(line, 3, "cursor is at correct line number in original source");

  finishUp();
}

/* Helpers */

function verifyLinkText(text, callback) {
  let label = getLinkByIndex(1).querySelector("label");

  waitForSuccess({
    name: "link text changed to display correct location: " + text,
    validatorFn: function()
    {
      return label.getAttribute("value") == text;
    },
    successFn: callback,
    failureFn: callback,
  });
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
  Services.prefs.clearUserPref(PREF);
  finish();
}
