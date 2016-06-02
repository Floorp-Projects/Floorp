/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the rendering of text nodes in the markup view.

const LONG_VALUE = "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do " +
  "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam.";
const SCHEMA = "data:text/html;charset=UTF-8,";
const TEST_URL = `${SCHEMA}<!DOCTYPE html>
  <html>
  <body>
    <div id="shorttext">Short text</div>
    <div id="longtext">${LONG_VALUE}</div>
    <div id="shortcomment"><!--Short comment--></div>
    <div id="longcomment"><!--${LONG_VALUE}--></div>
    <div id="shorttext-and-node">Short text<span>Other element</span></div>
    <div id="longtext-and-node">${LONG_VALUE}<span>Other element</span></div>
  </body>
  </html>`;

const TEST_DATA = [{
  desc: "Test node containing a short text, short text nodes can be inlined.",
  selector: "#shorttext",
  inline: true,
  value: "Short text",
}, {
  desc: "Test node containing a long text, long text nodes are not inlined.",
  selector: "#longtext",
  inline: false,
  value: LONG_VALUE,
}, {
  desc: "Test node containing a short comment, comments are not inlined.",
  selector: "#shortcomment",
  inline: false,
  value: "Short comment",
}, {
  desc: "Test node containing a long comment, comments are not inlined.",
  selector: "#longcomment",
  inline: false,
  value: LONG_VALUE,
}, {
  desc: "Test node containing a short text and a span.",
  selector: "#shorttext-and-node",
  inline: false,
  value: "Short text",
}, {
  desc: "Test node containing a long text and a span.",
  selector: "#longtext-and-node",
  inline: false,
  value: LONG_VALUE,
}, ];

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  for (let data of TEST_DATA) {
    yield checkNode(inspector, testActor, data);
  }
});

function* checkNode(inspector, testActor, {desc, selector, inline, value}) {
  info(desc);

  let container = yield getContainerForSelector(selector, inspector);
  let nodeValue = yield getFirstChildNodeValue(selector, testActor);
  is(nodeValue, value, "The test node's text content is correct");

  is(!!container.inlineTextChild, inline, "Container inlineTextChild is as expected");
  is(!container.canExpand, inline, "Container canExpand property is as expected");

  let textContainer;
  if (inline) {
    textContainer = container.elt.querySelector("pre");
    ok(!!textContainer, "Text container is already rendered for inline text elements");
  } else {
    textContainer = container.elt.querySelector("pre");
    ok(!textContainer, "Text container is not rendered for collapsed text nodes");
    yield inspector.markup.expandNode(container.node);
    yield waitForMultipleChildrenUpdates(inspector);

    textContainer = container.elt.querySelector("pre");
    ok(!!textContainer, "Text container is rendered after expanding the container");
  }

  is(textContainer.textContent, value, "The complete text node is rendered.");
}
