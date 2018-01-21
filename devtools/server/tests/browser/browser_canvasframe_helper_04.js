/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the CanvasFrameAnonymousContentHelper re-inserts the content when the
// page reloads.

const TEST_URL_1 =
  "data:text/html;charset=utf-8,CanvasFrameAnonymousContentHelper test 1";
const TEST_URL_2 =
  "data:text/html;charset=utf-8,CanvasFrameAnonymousContentHelper test 2";

add_task(async function () {
  let browser = await addTab(TEST_URL_1);
  await injectEventUtilsInContentTask(browser);
  await ContentTask.spawn(browser, TEST_URL_2, async function (url2) {
    const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
    const {HighlighterEnvironment} = require("devtools/server/actors/highlighters");
    const {
      CanvasFrameAnonymousContentHelper
    } = require("devtools/server/actors/highlighters/utils/markup");
    let doc = content.document;

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
    let onMouseDown = (e, id) => {
      is(id, "child-element", "The mousedown event was triggered on the element");
      mouseDownHandled++;
    };
    el.addEventListener("mousedown", onMouseDown);

    let once = function once(target, event) {
      return new Promise(done => {
        target.addEventListener(event, done, { once: true });
      });
    };

    let synthesizeMouseDown = function synthesizeMouseDown(x, y, win) {
      // We need to make sure the inserted anonymous content can be targeted by the
      // event right after having been inserted, and so we need to force a sync
      // reflow.
      win.document.documentElement.offsetWidth;
      EventUtils.synthesizeMouseAtPoint(x, y, {type: "mousedown"}, win);
    };

    info("Synthesizing an event on the element");
    let onDocMouseDown = once(doc, "mousedown");
    synthesizeMouseDown(100, 100, doc.defaultView);
    await onDocMouseDown;
    is(mouseDownHandled, 1, "The mousedown event was handled once before navigation");

    info("Navigating to a new page");
    let loaded = once(this, "load");
    content.location = url2;
    await loaded;

    // Wait for the next event tick to make sure the remaining part of the
    // test is not executed in the microtask checkpoint for load event
    // itself.  Otherwise the synthesizeMouseDown doesn't work.
    await new Promise(r => setTimeout(r, 0));

    // Update to the new document we just loaded
    doc = content.document;

    info("Try to access the element again");
    is(el.getAttribute("class"), "child-element",
      "The attribute is correct after navigation");
    is(el.getTextContent(), "test content",
      "The text content is correct after navigation");

    info("Synthesizing an event on the element again");
    onDocMouseDown = once(doc, "mousedown");
    synthesizeMouseDown(100, 100, doc.defaultView);
    await onDocMouseDown;
    is(mouseDownHandled, 1, "The mousedown event was not handled after navigation");

    info("Destroying the helper");
    env.destroy();
    helper.destroy();
  });

  gBrowser.removeCurrentTab();
});
