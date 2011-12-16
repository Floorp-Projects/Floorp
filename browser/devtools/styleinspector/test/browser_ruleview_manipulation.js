/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource:///modules/devtools/CssRuleView.jsm");

let doc;

function simpleOverride()
{
  doc.body.innerHTML = '<div id="testid">Styled Node</div>';
  let element = doc.getElementById("testid");
  let elementStyle = new _ElementStyle(element);

  let elementRule = elementStyle.rules[0];
  let firstProp = elementRule.createProperty("background-color", "green", "");
  let secondProp = elementRule.createProperty("background-color", "blue", "");
  is(elementRule.textProps[0], firstProp, "Rules should be in addition order.");
  is(elementRule.textProps[1], secondProp, "Rules should be in addition order.");

  is(element.style.getPropertyValue("background-color"), "blue", "Second property should have been used.");

  secondProp.remove();
  is(element.style.getPropertyValue("background-color"), "green", "After deleting second property, first should be used.");

  secondProp = elementRule.createProperty("background-color", "blue", "");
  is(element.style.getPropertyValue("background-color"), "blue", "New property should be used.");

  is(elementRule.textProps[0], firstProp, "Rules shouldn't have switched places.");
  is(elementRule.textProps[1], secondProp, "Rules shouldn't have switched places.");

  secondProp.setEnabled(false);
  is(element.style.getPropertyValue("background-color"), "green", "After disabling second property, first value should be used");

  firstProp.setEnabled(false);
  is(element.style.getPropertyValue("background-color"), "", "After disabling both properties, value should be empty.");

  secondProp.setEnabled(true);
  is(element.style.getPropertyValue("background-color"), "blue", "Value should be set correctly after re-enabling");

  firstProp.setEnabled(true);
  is(element.style.getPropertyValue("background-color"), "blue", "Re-enabling an earlier property shouldn't make it override a later property.");
  is(elementRule.textProps[0], firstProp, "Rules shouldn't have switched places.");
  is(elementRule.textProps[1], secondProp, "Rules shouldn't have switched places.");

  firstProp.setValue("purple", "");
  is(element.style.getPropertyValue("background-color"), "blue", "Modifying an earlier property shouldn't override a later property.");

  finishTest();
}

function finishTest()
{
  doc = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    waitForFocus(simpleOverride, content);
  }, true);

  content.location = "data:text/html,basic style inspector tests";
}
