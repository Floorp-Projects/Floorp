/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("sendQuery testing", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");
    ok(actorParent, "JSWindowActorParent should have value.");

    let { result } = await actorParent.sendQuery("asyncAdd", { a: 10, b: 20 });
    is(result, 30);
  },
});

declTest("sendQuery in-process early lifetime", {
  url: "about:mozilla",
  allFrames: true,

  async test(browser) {
    let iframe = browser.contentDocument.createElement("iframe");
    browser.contentDocument.body.appendChild(iframe);
    let wgc = iframe.contentWindow.getWindowGlobalChild();
    let actorChild = wgc.getActor("Test");
    let { result } = await actorChild.sendQuery("asyncMul", { a: 10, b: 20 });
    is(result, 200);
  },
});
