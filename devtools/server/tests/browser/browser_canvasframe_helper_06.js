/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test support for event propagation stop in the
// CanvasFrameAnonymousContentHelper event handling mechanism.

const TEST_URL = "data:text/html;charset=utf-8,CanvasFrameAnonymousContentHelper test";

add_task(async function() {
  const browser = await addTab(TEST_URL);
  await ContentTask.spawn(browser, null, async function() {
    const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
    const {HighlighterEnvironment} = require("devtools/server/actors/highlighters");
    const {
      CanvasFrameAnonymousContentHelper
    } = require("devtools/server/actors/highlighters/utils/markup");
    const doc = content.document;

    const nodeBuilder = () => {
      const root = doc.createElement("div");

      const parent = doc.createElement("div");
      parent.style = "pointer-events:auto;width:300px;height:300px;background:yellow;";
      parent.id = "parent-element";
      root.appendChild(parent);

      const child = doc.createElement("div");
      child.style = "pointer-events:auto;width:200px;height:200px;background:red;";
      child.id = "child-element";
      parent.appendChild(child);

      return root;
    };

    info("Building the helper");
    const env = new HighlighterEnvironment();
    env.initFromWindow(doc.defaultView);
    const helper = new CanvasFrameAnonymousContentHelper(env, nodeBuilder);

    info("Getting the parent and child elements");
    const parentEl = helper.getElement("parent-element");
    const childEl = helper.getElement("child-element");

    info("Adding an event listener on both elements");
    let mouseDownHandled = [];

    function onParentMouseDown(e, id) {
      mouseDownHandled.push(id);
    }
    parentEl.addEventListener("mousedown", onParentMouseDown);

    function onChildMouseDown(e, id) {
      mouseDownHandled.push(id);
      e.stopPropagation();
    }
    childEl.addEventListener("mousedown", onChildMouseDown);

    function once(target, event) {
      return new Promise(done => {
        target.addEventListener(event, done, { once: true });
      });
    }

    info("Synthesizing an event on the child element");
    let onDocMouseDown = once(doc, "mousedown");
    synthesizeMouseDown(100, 100, doc.defaultView);
    await onDocMouseDown;

    is(mouseDownHandled.length, 1, "The mousedown event was handled only once");
    is(mouseDownHandled[0], "child-element",
      "The mousedown event was handled on the child element");

    info("Synthesizing an event on the parent, outside of the child element");
    mouseDownHandled = [];
    onDocMouseDown = once(doc, "mousedown");
    synthesizeMouseDown(250, 250, doc.defaultView);
    await onDocMouseDown;

    is(mouseDownHandled.length, 1, "The mousedown event was handled only once");
    is(mouseDownHandled[0], "parent-element",
      "The mousedown event was handled on the parent element");

    info("Removing the event listener");
    parentEl.removeEventListener("mousedown", onParentMouseDown);
    childEl.removeEventListener("mousedown", onChildMouseDown);

    env.destroy();
    helper.destroy();

    function synthesizeMouseDown(x, y, win) {
      // We need to make sure the inserted anonymous content can be targeted by the
      // event right after having been inserted, and so we need to force a sync
      // reflow.
      win.document.documentElement.offsetWidth;
      // Minimal environment for EventUtils to work.
      const EventUtils = {
        window: content,
        parent: content,
        _EU_Ci: Ci,
        _EU_Cc: Cc,
      };
      Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);
      EventUtils.synthesizeMouseAtPoint(x, y, {type: "mousedown"}, win);
    }
  });

  gBrowser.removeCurrentTab();
});
