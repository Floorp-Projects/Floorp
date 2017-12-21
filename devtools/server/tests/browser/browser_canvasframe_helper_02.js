/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the CanvasFrameAnonymousContentHelper does not insert content in
// XUL windows.

add_task(async function () {
  let browser = await addTab("about:preferences");

  await ContentTask.spawn(browser, null, async function () {
    const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
    const {HighlighterEnvironment} = require("devtools/server/actors/highlighters");
    const {
      CanvasFrameAnonymousContentHelper
    } = require("devtools/server/actors/highlighters/utils/markup");
    let doc = content.document;

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

    ok(!helper.content, "The AnonymousContent was not inserted in the window");
    ok(!helper.getTextContentForElement("child-element"),
      "No text content is returned");

    env.destroy();
    helper.destroy();
  });

  gBrowser.removeCurrentTab();
});
