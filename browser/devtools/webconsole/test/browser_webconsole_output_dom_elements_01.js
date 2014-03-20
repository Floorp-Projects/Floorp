/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the webconsole output for various types of DOM Nodes.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-output-dom-elements.html";

let inputTests = [
  {
    input: "testBodyNode()",
    output: '<body id="body-id" class="body-class">',
    printOutput: "[object HTMLBodyElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testDocumentElement()",
    output: '<html lang="en-US" dir="ltr">',
    printOutput: "[object HTMLHtmlElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testDocument()",
    output: 'HTMLDocument \u2192 ' + TEST_URI,
    printOutput: "[object HTMLDocument]",
    inspectable: true,
    noClick: true,
    inspectorIcon: false
  },

  {
    input: "testNode()",
    output: '<p some-attribute="some-value">',
    printOutput: "[object HTMLParagraphElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testNodeList()",
    output: 'NodeList [ <html>, <head>, <meta>, <title>, <body#body-id.body-class>, <p>, <iframe>, <div.some.classname.here.with.more.classnames.here>, <script> ]',
    printOutput: "[object NodeList]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testNodeInIframe()",
    output: '<p>',
    printOutput: "[object HTMLParagraphElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testDocumentFragment()",
    output: 'DocumentFragment [ <span.foo>, <div#fragdiv> ]',
    printOutput: "[object DocumentFragment]",
    inspectable: true,
    noClick: true,
    inspectorIcon: false
  },

  {
    input: "testNodeInDocumentFragment()",
    output: '<span class="foo" data-lolz="hehe">',
    printOutput: "[object HTMLSpanElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: false
  },

  {
    input: "testUnattachedNode()",
    output: '<p class="such-class" data-data="such-data">',
    printOutput: "[object HTMLParagraphElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: false
  }
];

function test() {
  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    yield checkOutputForInputs(hud, inputTests);
  }).then(finishTest);
}
