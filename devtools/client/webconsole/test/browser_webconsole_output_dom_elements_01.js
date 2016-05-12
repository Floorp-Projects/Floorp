/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejections should be fixed.

"use strict";

thisTestLeaksUncaughtRejectionsAndShouldBeFixed(null);
thisTestLeaksUncaughtRejectionsAndShouldBeFixed(
  "TypeError: this.toolbox is null");

// Test the webconsole output for various types of DOM Nodes.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-output-dom-elements.html";

var inputTests = [
  {
    input: "testBodyNode()",
    output: '<body class="body-class" id="body-id">',
    printOutput: "[object HTMLBodyElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testDocumentElement()",
    output: '<html dir="ltr" lang="en-US">',
    printOutput: "[object HTMLHtmlElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testDocument()",
    output: "HTMLDocument \u2192 " + TEST_URI,
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
    output: "NodeList [ <p>, <p#lots-of-attributes>, <iframe>, " +
            "<div.some.classname.here.with.more.classnames.here>, " +
            "<svg>, <clipPath>, <rect>, <script> ]",
    printOutput: "[object NodeList]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testNodeInIframe()",
    output: "<p>",
    printOutput: "[object HTMLParagraphElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testLotsOfAttributes()",
    output: '<p id="lots-of-attributes" a="" b="" c="" d="" e="" f="" g="" ' +
            'h="" i="" j="" k="" l="" m="" n="">',
    printOutput: "[object HTMLParagraphElement]",
    inspectable: true,
    noClick: true,
    inspectorIcon: true
  },

  {
    input: "testDocumentFragment()",
    output: "DocumentFragment [ <span.foo>, <div#fragdiv> ]",
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
  },
];

function test() {
  requestLongerTimeout(2);
  Task.spawn(function* () {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    yield checkOutputForInputs(hud, inputTests);
  }).then(finishTest);
}
