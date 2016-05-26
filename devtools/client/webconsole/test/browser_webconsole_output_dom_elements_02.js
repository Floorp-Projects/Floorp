/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the inspector links in the webconsole output for DOM Nodes actually
// open the inspector and select the right node.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-output-dom-elements.html";

const TEST_DATA = [
  {
    // The first test shouldn't be returning the body element as this is the
    // default selected node, so re-selecting it won't fire the
    // inspector-updated event
    input: "testNode()",
    output: '<p some-attribute="some-value">',
    displayName: "p",
    attrs: [{name: "some-attribute", value: "some-value"}]
  },
  {
    input: "testBodyNode()",
    output: '<body class="body-class" id="body-id">',
    displayName: "body",
    attrs: [
      {
        name: "class", value: "body-class"
      },
      {
        name: "id", value: "body-id"
      }
    ]
  },
  {
    input: "testNodeInIframe()",
    output: "<p>",
    displayName: "p",
    attrs: []
  },
  {
    input: "testDocumentElement()",
    output: '<html dir="ltr" lang="en-US">',
    displayName: "html",
    attrs: [
      {
        name: "dir",
        value: "ltr"
      },
      {
        name: "lang",
        value: "en-US"
      }
    ]
  }
];

function test() {
  Task.spawn(function* () {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    yield checkDomElementHighlightingForInputs(hud, TEST_DATA);
  }).then(finishTest);
}
