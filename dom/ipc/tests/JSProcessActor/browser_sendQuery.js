/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ERROR_LINE_NUMBERS = {
  jsm: 33,
  "sys.mjs": 31,
};
const EXCEPTION_LINE_NUMBERS = {
  jsm: ERROR_LINE_NUMBERS.jsm + 3,
  "sys.mjs": ERROR_LINE_NUMBERS["sys.mjs"] + 3,
};
const ERROR_COLUMN_NUMBERS = {
  jsm: 31,
  "sys.mjs": 31,
};
const EXCEPTION_COLUMN_NUMBERS = {
  jsm: 22,
  "sys.mjs": 22,
};

function maybeAsyncStack(offset, column) {
  if (
    Services.prefs.getBoolPref(
      "javascript.options.asyncstack_capture_debuggee_only"
    )
  ) {
    return "";
  }

  let stack = Error().stack.replace(/^.*?\n/, "");
  return (
    "JSActor query*" +
    stack.replace(
      /^([^\n]+?):(\d+):\d+/,
      (m0, m1, m2) => `${m1}:${+m2 + offset}:${column}`
    )
  );
}

declTest("sendQuery Error", {
  async test(browser, _window, fileExt) {
    let parent = browser.browsingContext.currentWindowGlobal.domProcess;
    let actorParent = parent.getActor("TestProcessActor");

    let asyncStack = maybeAsyncStack(2, 8);
    let error = await actorParent
      .sendQuery("error", { message: "foo" })
      .catch(e => e);

    is(error.message, "foo", "Error should have the correct message");
    is(error.name, "SyntaxError", "Error should have the correct name");
    is(
      error.stack,
      `receiveMessage@resource://testing-common/TestProcessActorChild.${fileExt}:${ERROR_LINE_NUMBERS[fileExt]}:${ERROR_COLUMN_NUMBERS[fileExt]}\n` +
        asyncStack,
      "Error should have the correct stack"
    );
  },
});

declTest("sendQuery Exception", {
  async test(browser, _window, fileExt) {
    let parent = browser.browsingContext.currentWindowGlobal.domProcess;
    let actorParent = parent.getActor("TestProcessActor");

    let asyncStack = maybeAsyncStack(2, 8);
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
      `receiveMessage@resource://testing-common/TestProcessActorChild.${fileExt}:${EXCEPTION_LINE_NUMBERS[fileExt]}:${EXCEPTION_COLUMN_NUMBERS[fileExt]}\n` +
        asyncStack,
      "Error should have the correct stack"
    );
  },
});

declTest("sendQuery testing", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal.domProcess;
    let actorParent = parent.getActor("TestProcessActor");
    ok(actorParent, "JSWindowActorParent should have value.");

    let { result } = await actorParent.sendQuery("asyncAdd", { a: 10, b: 20 });
    is(result, 30);
  },
});
