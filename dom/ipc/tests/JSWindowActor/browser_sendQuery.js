/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("sendQuery Error", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");

    let error = await actorParent
      .sendQuery("error", { message: "foo" })
      .catch(e => e);

    is(error.message, "foo", "Error should have the correct message");
    is(error.name, "SyntaxError", "Error should have the correct name");
    is(
      error.stack,
      "receiveMessage@resource://testing-common/TestChild.jsm:28:31\n",
      "Error should have the correct stack"
    );
  },
});

declTest("sendQuery Exception", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");

    let error = await actorParent
      .sendQuery("exception", {
        message: "foo",
        result: Cr.NS_ERROR_INVALID_ARG,
      })
      .catch(e => e);

    is(error.message, "foo", "Error should have the correct message");
    is(
      error.result,
      Cr.NS_ERROR_INVALID_ARG,
      "Error should have the correct result code"
    );
    is(
      error.stack,
      "receiveMessage@resource://testing-common/TestChild.jsm:31:22\n",
      "Error should have the correct stack"
    );
  },
});

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
