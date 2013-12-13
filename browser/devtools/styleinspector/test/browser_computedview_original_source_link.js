/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let win;
let doc;
let inspector;
let computedView;
let toolbox;

const TESTCASE_URI = TEST_BASE_HTTPS + "sourcemaps.html";
const PREF = "devtools.styleeditor.source-maps-enabled";

function test()
{
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF, true);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    doc = content.document;
    waitForFocus(function () { openComputedView(highlightNode); }, content);
  }, true);

  content.location = TESTCASE_URI;
}

function highlightNode(aInspector, aComputedView)
{
  inspector = aInspector;
  computedView = aComputedView;

  // Highlight a node.
  let div = content.document.getElementsByTagName("div")[0];
  ok(div, "div to select exists")

  inspector.selection.setNode(div);
  inspector.once("inspector-updated", () => {
    is(inspector.selection.node, div, "selection matches the div element");

    expandProperty(0, testComputedViewLink);
  });
}

function testComputedViewLink() {
  let link = getLinkByIndex(0);
  waitForSuccess({
    name: "link text changed to display original source location",
    validatorFn: function()
    {
      return link.textContent == "sourcemaps.scss:4";
    },
    successFn: linkChanged,
    failureFn: linkChanged,
  });
}

function linkChanged() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = gDevTools.getToolbox(target);

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

  let link = getLinkByIndex(0);

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
function expandProperty(aIndex, aCallback)
{
  info("expanding property " + aIndex);
  let contentDoc = computedView.styleDocument;
  let expando = contentDoc.querySelectorAll(".expandable")[aIndex];

  expando.click();

  inspector.once("computed-view-property-expanded", aCallback);
}

function getLinkByIndex(aIndex)
{
  let contentDoc = computedView.styleDocument;
  let links = contentDoc.querySelectorAll(".rule-link .link");
  return links[aIndex];
}

function finishUp()
{
  gBrowser.removeCurrentTab();
  doc = inspector = computedView = toolbox = win = null;
  Services.prefs.clearUserPref(PREF);
  finish();
}
