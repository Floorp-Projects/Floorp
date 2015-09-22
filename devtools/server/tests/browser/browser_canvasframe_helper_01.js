/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Simple CanvasFrameAnonymousContentHelper tests.

// This makes sure the 'domnode' protocol actor type is known when importing
// highlighter.
require("devtools/server/actors/inspector");
const {HighlighterEnvironment} = require("devtools/server/actors/highlighters");

const {
  CanvasFrameAnonymousContentHelper
} = require("devtools/server/actors/highlighters/utils/markup");

const TEST_URL = "data:text/html;charset=utf-8,CanvasFrameAnonymousContentHelper test";

add_task(function*() {
  let doc = yield addTab(TEST_URL);

  let nodeBuilder = () => {
    let root = doc.createElement("div");
    let child = doc.createElement("div");
    child.style = "width:200px;height:200px;background:red;";
    child.id = "child-element";
    child.className = "child-element";
    child.textContent = "test element";
    root.appendChild(child);
    return root;
  };

  info("Building the helper");
  let env = new HighlighterEnvironment();
  env.initFromWindow(doc.defaultView);
  let helper = new CanvasFrameAnonymousContentHelper(env, nodeBuilder);

  ok(helper.content instanceof AnonymousContent,
    "The helper owns the AnonymousContent object");
  ok(helper.getTextContentForElement,
    "The helper has the getTextContentForElement method");
  ok(helper.setTextContentForElement,
    "The helper has the setTextContentForElement method");
  ok(helper.setAttributeForElement,
    "The helper has the setAttributeForElement method");
  ok(helper.getAttributeForElement,
    "The helper has the getAttributeForElement method");
  ok(helper.removeAttributeForElement,
    "The helper has the removeAttributeForElement method");
  ok(helper.addEventListenerForElement,
    "The helper has the addEventListenerForElement method");
  ok(helper.removeEventListenerForElement,
    "The helper has the removeEventListenerForElement method");
  ok(helper.getElement,
    "The helper has the getElement method");
  ok(helper.scaleRootElement,
    "The helper has the scaleRootElement method");

  is(helper.getTextContentForElement("child-element"), "test element",
    "The text content was retrieve correctly");
  is(helper.getAttributeForElement("child-element", "id"), "child-element",
    "The ID attribute was retrieve correctly");
  is(helper.getAttributeForElement("child-element", "class"), "child-element",
    "The class attribute was retrieve correctly");

  let el = helper.getElement("child-element");
  ok(el, "The DOMNode-like element was created");

  is(el.getTextContent(), "test element",
    "The text content was retrieve correctly");
  is(el.getAttribute("id"), "child-element",
    "The ID attribute was retrieve correctly");
  is(el.getAttribute("class"), "child-element",
    "The class attribute was retrieve correctly");

  info("Destroying the helper");
  helper.destroy();
  env.destroy();

  ok(!helper.getTextContentForElement("child-element"),
    "No text content was retrieved after the helper was destroyed");
  ok(!helper.getAttributeForElement("child-element", "id"),
    "No ID attribute was retrieved after the helper was destroyed");
  ok(!helper.getAttributeForElement("child-element", "class"),
    "No class attribute was retrieved after the helper was destroyed");

  gBrowser.removeCurrentTab();
});
