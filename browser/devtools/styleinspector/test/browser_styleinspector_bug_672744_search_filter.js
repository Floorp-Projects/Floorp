/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the search filter works properly.

let doc;
let inspector;
let computedView;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span id="matches" class="matches">Some styled text</span>' +
    '</div>';
  doc.title = "Style Inspector Search Filter Test";

  openComputedView(runStyleInspectorTests);
}

function runStyleInspectorTests(aInspector, aComputedView)
{
  inspector = aInspector;
  computedView = aComputedView;

  SI_inspectNode();
}

function SI_inspectNode()
{
  var span = doc.querySelector("#matches");
  ok(span, "captain, we have the matches span");

  inspector.selection.setNode(span);
  inspector.once("inspector-updated", () => {
    is(span, computedView.viewedElement.rawNode(),
      "style inspector node matches the selected node");
    SI_toggleDefaultStyles();
  });
}

function SI_toggleDefaultStyles()
{
  info("checking \"Browser styles\" checkbox");

  let doc = computedView.styleDocument;
  let checkbox = doc.querySelector(".includebrowserstyles");
  inspector.once("computed-view-refreshed", SI_AddFilterText);
  checkbox.click();
}

function SI_AddFilterText()
{
  let doc = computedView.styleDocument;
  let searchbar = doc.querySelector(".devtools-searchinput");
  inspector.once("computed-view-refreshed", SI_checkFilter);
  info("setting filter text to \"color\"");
  searchbar.focus();

  let win =computedView.styleWindow;
  EventUtils.synthesizeKey("c", {}, win);
  EventUtils.synthesizeKey("o", {}, win);
  EventUtils.synthesizeKey("l", {}, win);
  EventUtils.synthesizeKey("o", {}, win);
  EventUtils.synthesizeKey("r", {}, win);
}

function SI_checkFilter()
{
  let propertyViews = computedView.propertyViews;

  info("check that the correct properties are visible");
  propertyViews.forEach(function(propView) {
    let name = propView.name;
    is(propView.visible, name.indexOf("color") > -1,
      "span " + name + " property visibility check");
  });

  finishUp();
}

function finishUp()
{
  doc = inspector = computedView = null;
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
