/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the CanvasFrameAnonymousContentHelper does not insert content in
// XUL windows.

add_task(async function() {
  const browser = await addTab(
    "chrome://mochitests/content/browser/devtools/server/tests/browser/test-window.xhtml"
  );

  await SpecialPowers.spawn(browser, [], async function() {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const {
      HighlighterEnvironment,
    } = require("devtools/server/actors/highlighters");
    const {
      CanvasFrameAnonymousContentHelper,
    } = require("devtools/server/actors/highlighters/utils/markup");
    const doc = content.document;

    const nodeBuilder = () => {
      const root = doc.createElement("div");
      const child = doc.createElement("div");
      child.style = "width:200px;height:200px;background:red;";
      child.id = "child-element";
      child.className = "child-element";
      child.textContent = "test element";
      root.appendChild(child);
      return root;
    };

    info("Building the helper");
    const env = new HighlighterEnvironment();
    env.initFromWindow(doc.defaultView);
    const helper = new CanvasFrameAnonymousContentHelper(env, nodeBuilder);

    ok(!helper.content, "The AnonymousContent was not inserted in the window");
    ok(
      !helper.getTextContentForElement("child-element"),
      "No text content is returned"
    );

    env.destroy();
    helper.destroy();
  });

  gBrowser.removeCurrentTab();
});
