/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the checkbox to show only user styles works properly.

let doc;
let stylePanel;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span id="matches" class="matches">Some styled text</span>' +
    '</div>';
  doc.title = "Style Inspector Default Styles Test";
  ok(window.StyleInspector, "StyleInspector exists");
  // ok(StyleInspector.isEnabled, "style inspector preference is enabled");
  stylePanel = new StyleInspector(window);
  Services.obs.addObserver(runStyleInspectorTests, "StyleInspector-opened", false);
  stylePanel.createPanel(false, function() {
    stylePanel.open(doc.body);
  });
}

function runStyleInspectorTests()
{
  Services.obs.removeObserver(runStyleInspectorTests, "StyleInspector-opened", false);

  ok(stylePanel.isOpen(), "style inspector is open");

  Services.obs.addObserver(SI_check, "StyleInspector-populated", false);
  SI_inspectNode();
}

function SI_inspectNode()
{
  let span = doc.querySelector("#matches");
  ok(span, "captain, we have the matches span");

  let htmlTree = stylePanel.cssHtmlTree;
  stylePanel.selectNode(span);

  is(span, htmlTree.viewedElement,
    "style inspector node matches the selected node");
  is(htmlTree.viewedElement, stylePanel.cssLogic.viewedElement,
     "cssLogic node matches the cssHtmlTree node");
}

function SI_check()
{
  Services.obs.removeObserver(SI_check, "StyleInspector-populated", false);
  is(propertyVisible("color"), true,
    "span #matches color property is visible");
  is(propertyVisible("background-color"), false,
    "span #matches background-color property is hidden");

  SI_toggleDefaultStyles();
}

function SI_toggleDefaultStyles()
{
  // Click on the checkbox.
  let iframe = stylePanel.iframe;
  let checkbox = iframe.contentDocument.querySelector(".onlyuserstyles");
  Services.obs.addObserver(SI_checkDefaultStyles, "StyleInspector-populated", false);
  EventUtils.synthesizeMouse(checkbox, 5, 5, {}, iframe.contentWindow);
}

function SI_checkDefaultStyles()
{
  Services.obs.removeObserver(SI_checkDefaultStyles, "StyleInspector-populated", false);
  // Check that the default styles are now applied.
  is(propertyVisible("color"), true,
      "span color property is visible");
  is(propertyVisible("background-color"), true,
      "span background-color property is visible");

  Services.obs.addObserver(finishUp, "StyleInspector-closed", false);
  stylePanel.close();
}

function propertyVisible(aName)
{
  info("Checking property visibility for " + aName);
  let propertyViews = stylePanel.cssHtmlTree.propertyViews;
  for each (let propView in propertyViews) {
    if (propView.name == aName) {
      return propView.visible;
    }
  }
}

function finishUp()
{
  Services.obs.removeObserver(finishUp, "StyleInspector-closed", false);
  ok(!stylePanel.isOpen(), "style inspector is closed");
  doc = stylePanel = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,default styles test";
}
