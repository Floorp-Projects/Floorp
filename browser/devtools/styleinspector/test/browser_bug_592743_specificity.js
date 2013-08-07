/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that CSS specificity is properly calculated.

const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
                   .getService(Ci.inIDOMUtils);

function createDocument()
{
  let doc = content.document;
  doc.body.innerHTML = getStylesheetText();
  doc.title = "Computed view specificity test";
  runTests(doc);
}

function runTests(doc) {
  let cssLogic = new CssLogic();
  cssLogic.highlight(doc.body);

  let tests = getTests();
  let cssSheet = cssLogic.sheets[0];
  let cssRule = cssSheet.domSheet.cssRules[0];
  let selectors = CssLogic.getSelectors(cssRule);

  for (let i = 0; i < selectors.length; i++) {
    let selectorText = selectors[i];
    let selector = new CssSelector(cssRule, selectorText, i);
    let expected = getExpectedSpecificity(selectorText);
    let specificity = DOMUtils.getSpecificity(selector.cssRule,
                                              selector.selectorIndex)
    is(specificity, expected,
      'selector "' + selectorText + '" has a specificity of ' + expected);
  }
  finishUp();
}

function getExpectedSpecificity(selectorText) {
  let tests = getTests();

  for (let test of tests) {
    if (test.text == selectorText) {
      return test.expected;
    }
  }
}

function getTests() {
  return [
    {text: "*", expected: 0},
    {text: "LI", expected: 1},
    {text: "UL LI", expected: 2},
    {text: "UL OL + LI", expected: 3},
    {text: "H1 + [REL=\"up\"]", expected: 257},
    {text: "UL OL LI.red", expected: 259},
    {text: "LI.red.level", expected: 513},
    {text: ".red .level", expected: 512},
    {text: "#x34y", expected: 65536},
    {text: "#s12:not(FOO)", expected: 65537},
    {text: "body#home div#warning p.message", expected: 131331},
    {text: "* body#home div#warning p.message", expected: 131331},
    {text: "#footer :not(nav) li", expected: 65538},
    {text: "bar:nth-child(n)", expected: 257},
    {text: "li::-moz-list-number", expected: 1},
    {text: "a:hover", expected: 257},
  ];
}

function getStylesheetText() {
  let tests = getTests();
  let text = "";

  tests.forEach(function(test) {
    if (text.length > 0) {
      text += ",";
    }
    text += test.text;
  });
  return '<style type="text/css">' + text + " {color:red;}</style>";
}

function finishUp()
{
  CssLogic = CssSelector = tempScope = null;
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,Computed view specificity test";
}
