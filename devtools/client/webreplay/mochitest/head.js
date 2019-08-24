/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers.js",
  this
);

const EXAMPLE_URL =
  "http://example.com/browser/devtools/client/webreplay/mochitest/examples/";

// Attach a debugger to a tab, returning a promise that resolves with the
// debugger's toolbox.
async function attachDebugger(tab) {
  const target = await TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "jsdebugger");
  return { toolbox, tab, target };
}

async function attachRecordingDebugger(
  url,
  { waitForRecording, disableLogging } = {}
) {
  if (!disableLogging) {
    await pushPref("devtools.recordreplay.logging", true);
  }

  const tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + url, "current");

  if (waitForRecording) {
    await once(Services.ppmm, "RecordingFinished");
  }
  const { target, toolbox } = await attachDebugger(tab);
  const dbg = createDebuggerContext(toolbox);
  const threadFront = dbg.toolbox.threadFront;

  await threadFront.interrupt();
  return { ...dbg, tab, threadFront, target };
}

async function shutdownDebugger(dbg) {
  await waitForRequestsToSettle(dbg);
  await dbg.toolbox.destroy();
  await gBrowser.removeTab(dbg.tab);
}

// Return a promise that resolves when a breakpoint has been set.
async function setBreakpoint(threadFront, expectedFile, lineno, options = {}) {
  const { sources } = await threadFront.getSources();
  ok(sources.length == 1, "Got one source");
  ok(RegExp(expectedFile).test(sources[0].url), "Source is " + expectedFile);
  const location = { sourceUrl: sources[0].url, line: lineno };
  await threadFront.setBreakpoint(location, options);
  return location;
}

function resumeThenPauseAtLineFunctionFactory(method) {
  return async function(threadFront, lineno) {
    threadFront[method]();
    await threadFront.once("paused");

    const { frames } = await threadFront.getFrames(0, 1);
    const frameLine = frames[0] ? frames[0].where.line : undefined;
    ok(
      frameLine == lineno,
      "Paused at line " + frameLine + " expected " + lineno
    );
  };
}

// Define various methods that resume a thread in a specific way and ensure it
// pauses at a specified line.
var rewindToLine = resumeThenPauseAtLineFunctionFactory("rewind");
var resumeToLine = resumeThenPauseAtLineFunctionFactory("resume");
var reverseStepOverToLine = resumeThenPauseAtLineFunctionFactory(
  "reverseStepOver"
);
var stepOverToLine = resumeThenPauseAtLineFunctionFactory("stepOver");
var stepInToLine = resumeThenPauseAtLineFunctionFactory("stepIn");
var stepOutToLine = resumeThenPauseAtLineFunctionFactory("stepOut");

// Return a promise that resolves when a thread evaluates a string in the
// topmost frame, with the result throwing an exception.
async function checkEvaluateInTopFrameThrows(target, text) {
  const threadFront = target.threadFront;
  const consoleFront = await target.getFront("console");
  const { frames } = await threadFront.getFrames(0, 1);
  ok(frames.length == 1, "Got one frame");
  const options = { thread: threadFront.actor, frameActor: frames[0].actor };
  const response = await consoleFront.evaluateJS(text, options);
  ok(response.exception, "Eval threw an exception");
}

// Return a pathname that can be used for a new recording file.
function newRecordingFile() {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  return OS.Path.join(
    OS.Constants.Path.tmpDir,
    "MochitestRecording" + Math.round(Math.random() * 1000000000)
  );
}

function findMessage(hud, text, selector = ".message") {
  return findMessages(hud, text, selector)[0];
}

function findMessages(hud, text, selector = ".message") {
  const messages = hud.ui.outputNode.querySelectorAll(selector);
  const elements = Array.prototype.filter.call(messages, el =>
    el.textContent.includes(text)
  );

  if (elements.length == 0) {
    return null;
  }

  return elements;
}

function waitForMessages(hud, text, selector = ".message") {
  return waitUntilPredicate(() => findMessages(hud, text, selector));
}

async function waitForMessageCount(hud, text, length, selector = ".message") {
  let messages;
  await waitUntil(() => {
    messages = findMessages(hud, text, selector);
    return messages && messages.length == length;
  });
  ok(messages.length == length, "Found expected message count");
  return messages;
}

async function warpToMessage(hud, dbg, text) {
  let messages = await waitForMessages(hud, text);
  ok(messages.length == 1, "Found one message");
  const message = messages.pop();

  const menuPopup = await openConsoleContextMenu(message);
  console.log(`.>> menu`, menuPopup);

  const timeWarpItem = menuPopup.querySelector("#console-menu-time-warp");
  ok(timeWarpItem, "Time warp menu item is available");

  timeWarpItem.click();

  await hideConsoleContextMenu();
  await once(Services.ppmm, "TimeWarpFinished");
  await waitForPaused(dbg);

  messages = findMessages(hud, "", ".paused");
  ok(messages.length == 1, "Found one paused message");

  return message;

  async function openConsoleContextMenu(element) {
    const onConsoleMenuOpened = hud.ui.wrapper.once("menu-open");
    synthesizeContextMenuEvent(element);
    await onConsoleMenuOpened;
    return dbg.toolbox.topDoc.getElementById("webconsole-menu");
  }

  function hideConsoleContextMenu() {
    const popup = dbg.toolbox.topDoc.getElementById("webconsole-menu");
    if (!popup) {
      return Promise.resolve();
    }

    const onPopupHidden = once(popup, "popuphidden");
    popup.hidePopup();
    return onPopupHidden;
  }
}

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/NS_ERROR_NOT_INITIALIZED/);

// Many web replay tests can resume execution before the debugger has finished
// all operations related to the pause.
PromiseTestUtils.whitelistRejectionsGlobally(
  /Current thread has paused or resumed/
);

// When running the full test suite, long delays can occur early on in tests,
// before child processes have even been spawned. Allow a longer timeout to
// avoid failures from this.
requestLongerTimeout(4);
