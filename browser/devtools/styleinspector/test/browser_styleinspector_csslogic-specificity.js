/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS specificity is properly calculated.

const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
                   .getService(Ci.inIDOMUtils);
const TEST_DATA = [
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
  {text: "a:hover", expected: 257}
];

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,Computed view specificity test");
  createDocument();

  info("Creating a CssLogic instance");
  let cssLogic = new CssLogic();
  cssLogic.highlight(content.document.body);
  let cssSheet = cssLogic.sheets[0];
  let cssRule = cssSheet.domSheet.cssRules[0];
  let selectors = CssLogic.getSelectors(cssRule);

  info("Iterating over the test selectors")
  for (let i = 0; i < selectors.length; i++) {
    let selectorText = selectors[i];
    info("Testing selector " + selectorText);

    let selector = new CssSelector(cssRule, selectorText, i);
    let expected = getExpectedSpecificity(selectorText);
    let specificity = DOMUtils.getSpecificity(selector.cssRule,
                                              selector.selectorIndex)
    is(specificity, expected,
      'Selector "' + selectorText + '" has a specificity of ' + expected);
  }
});

function createDocument() {
  let doc = content.document;
  doc.body.innerHTML = getStylesheetText();
  doc.title = "Computed view specificity test";
}

function getStylesheetText() {
  info("Creating the test stylesheet");
  let text = TEST_DATA.map(i=>i.text).join(",");
  return '<style type="text/css">' + text + " {color:red;}</style>";
}

function getExpectedSpecificity(selectorText) {
  return TEST_DATA.filter(i=>i.text === selectorText)[0].expected;
}
