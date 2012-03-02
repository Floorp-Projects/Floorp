/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that CSS specificity is properly calculated.

let tempScope = {};
Cu.import("resource:///modules/devtools/CssLogic.jsm", tempScope);
let CssLogic = tempScope.CssLogic;
let CssSelector = tempScope.CssSelector;

function test()
{
  let tests = [
    {text: "*", expected: "000"},
    {text: "LI", expected: "001"},
    {text: "UL LI", expected: "002"},
    {text: "UL OL+LI", expected: "003"},
    {text: "H1 + *[REL=up]", expected: "011"},
    {text: "UL OL LI.red", expected: "013"},
    {text: "LI.red.level", expected: "021"},
    {text: ".red .level", expected: "020"},
    {text: "#x34y", expected: "100"},
    {text: "#s12:not(FOO)", expected: "101"},
    {text: "body#home div#warning p.message", expected: "213"},
    {text: "* body#home div#warning p.message", expected: "213"},
    {text: "#footer *:not(nav) li", expected: "102"},
    {text: "bar:nth-child(1n+0)", expected: "011"},
    {text: "li::-moz-list-number", expected: "002"},
    {text: "a:hover", expected: "011"},
  ];

  tests.forEach(function(aTest) {
    let selector = new CssSelector(null, aTest.text);
    let specificity = selector.specificity;

    let result = "" + specificity.ids + specificity.classes + specificity.tags;
    is(result, aTest.expected, "selector \"" + aTest.text +
      "\" produces expected result");
  });

  finishUp();
}

function finishUp()
{
  CssLogic = CssSelector = null;
  finish();
}
