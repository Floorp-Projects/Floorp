/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the search filter works properly.

let doc;
let stylePanel;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span id="matches" class="matches">Some styled text</span>' +
    '</div>';
  doc.title = "Style Inspector Search Filter Test";
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

  Services.obs.addObserver(SI_toggleDefaultStyles, "StyleInspector-populated", false);
  SI_inspectNode();
}

function SI_inspectNode()
{
  var span = doc.querySelector("#matches");
  ok(span, "captain, we have the matches span");

  let htmlTree = stylePanel.cssHtmlTree;
  stylePanel.selectNode(span);

  is(span, htmlTree.viewedElement,
    "style inspector node matches the selected node");
  is(htmlTree.viewedElement, stylePanel.cssLogic.viewedElement,
     "cssLogic node matches the cssHtmlTree node");
}

function SI_toggleDefaultStyles()
{
  Services.obs.removeObserver(SI_toggleDefaultStyles, "StyleInspector-populated", false);

  info("clearing \"only user styles\" checkbox");

  let iframe = stylePanel.iframe;
  let checkbox = iframe.contentDocument.querySelector(".onlyuserstyles");
  Services.obs.addObserver(SI_AddFilterText, "StyleInspector-populated", false);
  EventUtils.synthesizeMouse(checkbox, 5, 5, {}, iframe.contentWindow);
}

function SI_AddFilterText()
{
  Services.obs.removeObserver(SI_AddFilterText, "StyleInspector-populated", false);

  let iframe = stylePanel.iframe;
  let searchbar = iframe.contentDocument.querySelector(".searchfield");
  Services.obs.addObserver(SI_checkFilter, "StyleInspector-populated", false);
  info("setting filter text to \"color\"");
  searchbar.focus();
  EventUtils.synthesizeKey("c", {}, iframe.contentWindow);
  EventUtils.synthesizeKey("o", {}, iframe.contentWindow);
  EventUtils.synthesizeKey("l", {}, iframe.contentWindow);
  EventUtils.synthesizeKey("o", {}, iframe.contentWindow);
  EventUtils.synthesizeKey("r", {}, iframe.contentWindow);
}

function SI_checkFilter()
{
  Services.obs.removeObserver(SI_checkFilter, "StyleInspector-populated", false);
  let propertyViews = stylePanel.cssHtmlTree.propertyViews;

  info("check that the correct properties are visible");
  propertyViews.forEach(function(propView) {
    let name = propView.name;
    is(propView.visible, name.indexOf("color") > -1,
      "span " + name + " property visibility check");
  });

  Services.obs.addObserver(finishUp, "StyleInspector-closed", false);
  stylePanel.close();
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
