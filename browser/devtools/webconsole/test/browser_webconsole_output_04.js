/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("null");

// Test the webconsole output for various types of objects.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-output-04.html";

let inputTests = [
  // 0
  {
    input: "testTextNode()",
    output: '#text "hello world!"',
    printOutput: "[object Text]",
    inspectable: true,
    noClick: true,
  },

  // 1
  {
    input: "testCommentNode()",
    output: /<!--\s+- Any copyright /,
    printOutput: "[object Comment]",
    inspectable: true,
    noClick: true,
  },

  // 2
  {
    input: "testDocumentFragment()",
    output: 'DocumentFragment [ <div#foo1.bar>, <div#foo3> ]',
    printOutput: "[object DocumentFragment]",
    inspectable: true,
    variablesViewLabel: "DocumentFragment[2]",
  },

  // 3
  {
    input: "testError()",
    output: "TypeError: window.foobar is not a function\n" +
            "Stack trace:\n" +
            "testError@" + TEST_URI + ":44",
    printOutput: '"TypeError: window.foobar is not a function"',
    inspectable: true,
    variablesViewLabel: "TypeError",
  },

  // 4
  {
    input: "testDOMException()",
    output: 'DOMException [SyntaxError: "An invalid or illegal string was specified"',
    printOutput: '"SyntaxError: An invalid or illegal string was specified"',
    inspectable: true,
    variablesViewLabel: "SyntaxError",
  },

  // 5
  {
    input: "testCSSStyleDeclaration()",
    output: 'CSS2Properties { color: "green", font-size: "2em" }',
    printOutput: "[object CSS2Properties]",
    inspectable: true,
    noClick: true,
  },

  // 6
  {
    input: "testStyleSheetList()",
    output: "StyleSheetList [ CSSStyleSheet ]",
    printOutput: "[object StyleSheetList",
    inspectable: true,
    variablesViewLabel: "StyleSheetList[1]",
  },

  // 7
  {
    input: "document.styleSheets[0]",
    output: "CSSStyleSheet",
    printOutput: "[object CSSStyleSheet]",
    inspectable: true,
  },

  // 8
  {
    input: "document.styleSheets[0].cssRules",
    output: "CSSRuleList [ CSSStyleRule, CSSMediaRule ]",
    printOutput: "[object CSSRuleList",
    inspectable: true,
    variablesViewLabel: "CSSRuleList[2]",
  },

  // 9
  {
    input: "document.styleSheets[0].cssRules[0]",
    output: 'CSSStyleRule "p, div"',
    printOutput: "[object CSSStyleRule",
    inspectable: true,
    variablesViewLabel: "CSSStyleRule",
  },

  // 10
  {
    input: "document.styleSheets[0].cssRules[1]",
    output: 'CSSMediaRule "print"',
    printOutput: "[object CSSMediaRule",
    inspectable: true,
    variablesViewLabel: "CSSMediaRule",
  },
];

function test() {
  Task.spawn(function*() {
    const {tab} = yield loadTab(TEST_URI);
    const hud = yield openConsole(tab);
    yield checkOutputForInputs(hud, inputTests);
    inputTests = null;
  }).then(finishTest);
}
