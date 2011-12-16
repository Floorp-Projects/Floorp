/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for selector text errors.

let doc;
let stylePanel;

function createDocument()
{
  doc.body.innerHTML = "<div style='color:blue;'></div>";

  doc.title = "Style Inspector Selector Text Test";
  ok(window.StyleInspector, "StyleInspector exists");
  stylePanel = new StyleInspector(window);


  stylePanel.createPanel(false, function() {
    Services.obs.addObserver(SI_checkText, "StyleInspector-populated", false);

    let span = doc.querySelector("div");
    ok(span, "captain, we have the test div");
    stylePanel.open(span);
  });
}

function SI_checkText()
{
  Services.obs.removeObserver(SI_checkText, "StyleInspector-populated", false);

  let propertyView = null;
  stylePanel.cssHtmlTree.propertyViews.some(function(aView) {
    if (aView.name == "color") {
      propertyView = aView;
      return true;
    }
  });

  ok(propertyView, "found PropertyView for color");

  is(propertyView.hasMatchedSelectors, true, "hasMatchedSelectors is true");

  propertyView.matchedExpanded = true;
  propertyView.refreshMatchedSelectors();

  let td = propertyView.matchedSelectorsContainer.querySelector("td.rule-text");
  ok(td, "found the first table row");

  let selector = propertyView.matchedSelectorViews[0];
  ok(selector, "found the first matched selector view");

  try {
    is(td.textContent.trim(), selector.humanReadableText(td).trim(),
      "selector text is correct");
  } catch (ex) {
    info("EXCEPTION: " + ex);
    ok(false, "getting the selector text should not raise an exception");
  }

  Services.obs.addObserver(finishUp, "StyleInspector-closed", false);
  stylePanel.close();
}

function finishUp()
{
  Services.obs.removeObserver(finishUp, "StyleInspector-closed", false);
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

  content.location = "data:text/html,selector text test, bug 692400";
}
