/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the style inspector works properly

let doc;
let stylePanel;

const TEST_URI = "http://example.com/browser/browser/devtools/styleinspector/test/browser/browser_bug683672.html";

Cu.import("resource:///modules/devtools/CssHtmlTree.jsm");

function test()
{
  waitForExplicitFinish();
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded()
{
  ok(window.StyleInspector, "StyleInspector exists");
  ok(StyleInspector.isEnabled, "style inspector preference is enabled");
  stylePanel = StyleInspector.createPanel();
  Services.obs.addObserver(runTests, "StyleInspector-opened", false);
  stylePanel.openPopup();
}

function runTests()
{
  Services.obs.removeObserver(runTests, "StyleInspector-opened", false);

  ok(stylePanel.isOpen(), "style inspector is open");

  testMatchedSelectors();
  testUnmatchedSelectors();

  info("finishing up");
  Services.obs.addObserver(finishUp, "StyleInspector-closed", false);
  stylePanel.hidePopup();
}

function testMatchedSelectors()
{
  info("checking selector counts, matched rules and titles");
  let div = content.document.getElementById("test");
  ok(div, "captain, we have the div");

  info("selecting the div");
  stylePanel.selectNode(div);

  let htmlTree = stylePanel.cssHtmlTree;

  is(div, htmlTree.viewedElement,
      "style inspector node matches the selected node");

  let propertyView = new PropertyView(htmlTree, "color");
  let numMatchedSelectors = propertyView.propertyInfo.matchedSelectors.length;

  is(numMatchedSelectors, 6,
      "CssLogic returns the correct number of matched selectors for div");

  let dummy = content.document.getElementById("dummy");
  let returnedRuleTitle = propertyView.ruleTitle(dummy);
  let str = CssHtmlTree.l10n("property.numberOfSelectors");
  let calculatedRuleTitle = PluralForm.get(numMatchedSelectors, str)
                                      .replace("#1", numMatchedSelectors);

  info("returnedRuleTitle: '" + returnedRuleTitle + "'");

  is(returnedRuleTitle, calculatedRuleTitle,
      "returned title for matched rules is correct");
}

function testUnmatchedSelectors()
{
  info("checking selector counts, unmatched rules and titles");
  let body = content.document.body;
  ok(body, "captain, we have a body");

  info("selecting content.document.body");
  stylePanel.selectNode(body);

  let htmlTree = stylePanel.cssHtmlTree;

  is(body, htmlTree.viewedElement,
      "style inspector node matches the selected node");

  let propertyView = new PropertyView(htmlTree, "color");
  let numUnmatchedSelectors = propertyView.propertyInfo.unmatchedSelectors.length;

  is(numUnmatchedSelectors, 13,
      "CssLogic returns the correct number of unmatched selectors for body");

  let dummy = content.document.getElementById("dummy");
  let returnedRuleTitle = propertyView.ruleTitle(dummy);
  let str = CssHtmlTree.l10n("property.numberOfUnmatchedSelectors");
  let calculatedRuleTitle = PluralForm.get(numUnmatchedSelectors, str)
                                      .replace("#1", numUnmatchedSelectors);

  info("returnedRuleTitle: '" + returnedRuleTitle + "'");

  is(returnedRuleTitle, calculatedRuleTitle,
      "returned title for unmatched rules is correct");
}

function finishUp()
{
  Services.obs.removeObserver(finishUp, "StyleInspector-closed", false);
  ok(!stylePanel.isOpen(), "style inspector is closed");
  doc = stylePanel = null;
  gBrowser.removeCurrentTab();
  finish();
}
