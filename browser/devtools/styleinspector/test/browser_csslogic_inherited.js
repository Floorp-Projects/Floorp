/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that inherited properties are treated correctly.

Cu.import("resource:///modules/devtools/CssLogic.jsm");

let doc;

function createDocument()
{
  doc.body.innerHTML = '<div style="margin-left:10px; font-size: 5px"><div id="innerdiv">Inner div</div></div>';
  doc.title = "Style Inspector Inheritance Test";

  let cssLogic = new CssLogic();
  cssLogic.highlight(doc.getElementById("innerdiv"));

  let marginProp = cssLogic.getPropertyInfo("margin-left");
  is(marginProp.matchedRuleCount, 0, "margin-left should not be included in matched selectors.");

  let fontSizeProp = cssLogic.getPropertyInfo("font-size");
  is(fontSizeProp.matchedRuleCount, 1, "font-size should be included in matched selectors.");

  finishUp();
}

function finishUp()
{
  doc = null;
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
