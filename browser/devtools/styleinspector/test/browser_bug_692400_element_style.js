/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for selector text errors.

let doc;
let computedView;

function createDocument()
{
  doc.body.innerHTML = "<div style='color:blue;'></div>";

  doc.title = "Style Inspector Selector Text Test";

  openInspector(openComputedView);
}


function openComputedView(aInspector)
{
  let div = doc.querySelector("div");
  ok(div, "captain, we have the test div");

  aInspector.selection.setNode(div);

  aInspector.sidebar.once("computedview-ready", function() {
    aInspector.sidebar.select("computedview");
    computedView = getComputedView(aInspector);

    Services.obs.addObserver(SI_checkText, "StyleInspector-populated", false);
  });
}

function SI_checkText()
{
  Services.obs.removeObserver(SI_checkText, "StyleInspector-populated", false);

  let propertyView = null;
  computedView.propertyViews.some(function(aView) {
    if (aView.name == "color") {
      propertyView = aView;
      return true;
    }
    return false;
  });

  ok(propertyView, "found PropertyView for color");

  is(propertyView.hasMatchedSelectors, true, "hasMatchedSelectors is true");

  propertyView.matchedExpanded = true;
  propertyView.refreshMatchedSelectors();

  let span = propertyView.matchedSelectorsContainer.querySelector("span.rule-text");
  ok(span, "found the first table row");

  let selector = propertyView.matchedSelectorViews[0];
  ok(selector, "found the first matched selector view");

  finishUp();
}

function finishUp()
{
  doc = computedView = null;
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

  content.location = "data:text/html,selector text test, bug 692400";
}
