/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the CanvasFrameAnonymousContentHelper re-inserts the content when the
// page reloads.

// This makes sure the 'domnode' protocol actor type is known when importing
// highlighter.
require("devtools/server/actors/inspector");
const events = require("sdk/event/core");

const {HighlighterEnvironment} = require("devtools/server/actors/highlighters");

const {
  CanvasFrameAnonymousContentHelper
} = require("devtools/server/actors/highlighters/utils/markup");

const TEST_URL_1 = "data:text/html;charset=utf-8,CanvasFrameAnonymousContentHelper test 1";
const TEST_URL_2 = "data:text/html;charset=utf-8,CanvasFrameAnonymousContentHelper test 2";

add_task(function* () {
  let browser = yield addTab(TEST_URL_2);
  let doc = browser.contentDocument;

  let nodeBuilder = () => {
    let root = doc.createElement("div");
    let child = doc.createElement("div");
    child.style = "pointer-events:auto;width:200px;height:200px;background:red;";
    child.id = "child-element";
    child.className = "child-element";
    child.textContent = "test content";
    root.appendChild(child);
    return root;
  };

  info("Building the helper");
  let env = new HighlighterEnvironment();
  env.initFromWindow(doc.defaultView);
  let helper = new CanvasFrameAnonymousContentHelper(env, nodeBuilder);

  info("Get an element from the helper");
  let el = helper.getElement("child-element");

  info("Try to access the element");
  is(el.getAttribute("class"), "child-element",
    "The attribute is correct before navigation");
  is(el.getTextContent(), "test content",
    "The text content is correct before navigation");

  info("Add an event listener on the element");
  let mouseDownHandled = 0;
  function onMouseDown(e, id) {
    is(id, "child-element", "The mousedown event was triggered on the element");
    mouseDownHandled++;
  }
  el.addEventListener("mousedown", onMouseDown);

  info("Synthesizing an event on the element");
  let onDocMouseDown = once(doc, "mousedown");
  synthesizeMouseDown(100, 100, doc.defaultView);
  yield onDocMouseDown;
  is(mouseDownHandled, 1, "The mousedown event was handled once before navigation");

  info("Navigating to a new page");
  let loaded = once(gBrowser.selectedBrowser, "load", true);
  content.location = TEST_URL_2;
  yield loaded;
  doc = gBrowser.selectedBrowser.contentWindow.document;

  info("Try to access the element again");
  is(el.getAttribute("class"), "child-element",
    "The attribute is correct after navigation");
  is(el.getTextContent(), "test content",
    "The text content is correct after navigation");

  info("Synthesizing an event on the element again");
  onDocMouseDown = once(doc, "mousedown");
  synthesizeMouseDown(100, 100, doc.defaultView);
  yield onDocMouseDown;
  is(mouseDownHandled, 1, "The mousedown event was not handled after navigation");

  info("Destroying the helper");
  env.destroy();
  helper.destroy();

  gBrowser.removeCurrentTab();
});

function synthesizeMouseDown(x, y, win) {
  // We need to make sure the inserted anonymous content can be targeted by the
  // event right after having been inserted, and so we need to force a sync
  // reflow.
  let forceReflow = win.document.documentElement.offsetWidth;
  EventUtils.synthesizeMouseAtPoint(x, y, {type: "mousedown"}, win);
}
