/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * The Mochitest API documentation
 * @module mochitest
 */

/**
 * The mochitest API to wait for certain events.
 * @module mochitest/waits
 * @parent mochitest
 */

/**
 * The mochitest API predefined asserts.
 * @module mochitest/asserts
 * @parent mochitest
 */

/**
 * The mochitest API for interacting with the debugger.
 * @module mochitest/actions
 * @parent mochitest
 */

/**
 * Helper methods for the mochitest API.
 * @module mochitest/helpers
 * @parent mochitest
 */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/new/test/mochitest/helpers.js",
  this
);

const EXAMPLE_URL =
  "http://example.com/browser/devtools/client/debugger/new/test/mochitest/examples/";


async function waitUntilPredicate(predicate) {
  let result;
  await waitUntil(() => {
    result = predicate();
    return result;
  })

  return result;
}

// NOTE: still experimental, the screenshots might not be exactly correct
async function takeScreenshot(dbg) {
  let canvas = dbg.win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "html:canvas"
  );
  let context = canvas.getContext("2d");
  canvas.width = dbg.win.innerWidth;
  canvas.height = dbg.win.innerHeight;
  context.drawWindow(dbg.win, 0, 0, canvas.width, canvas.height, "white");
  await waitForTime(1000);
  dump(`[SCREENSHOT] ${canvas.toDataURL()}\n`);
}

// Attach a debugger to a tab, returning a promise that resolves with the
// debugger's toolbox.
async function attachDebugger(tab) {
  let target = await TargetFactory.forTab(tab);
  let toolbox = await gDevTools.showToolbox(target, "jsdebugger");
  return toolbox;
}

async function attachRecordingDebugger(url, { waitForRecording } = { waitForRecording: false }) {
  let tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + url, "current");
  
  if (waitForRecording) {
    await once(Services.ppmm, "RecordingFinished");
  }
  const toolbox = await attachDebugger(tab);
  const dbg = createDebuggerContext(toolbox)
  const threadClient = dbg.toolbox.threadClient;

  await threadClient.interrupt();
  return {...dbg, tab, threadClient};
}


// Return a promise with a reference to jsterm, opening the split
// console if necessary.  This cleans up the split console pref so
// it won't pollute other tests.
async function getSplitConsole(dbg) {
  const { toolbox, win } = dbg;

  if (!win) {
    win = toolbox.win;
  }

  if (!toolbox.splitConsole) {
    pressKey(dbg, "Escape");
  }

  await toolbox.openSplitConsole();
  return toolbox.getPanel("webconsole");
}


// Return a promise that resolves when a breakpoint has been set.
async function setBreakpoint(threadClient, expectedFile, lineno) {
  let {sources} = await threadClient.getSources();
  ok(sources.length == 1, "Got one source");
  ok(RegExp(expectedFile).test(sources[0].url), "Source is " + expectedFile);
  let sourceClient = threadClient.source(sources[0]);
  await sourceClient.setBreakpoint({ line: lineno });
}

function resumeThenPauseAtLineFunctionFactory(method) {
  return async function(threadClient, lineno) {
    threadClient[method]();
    await threadClient.addOneTimeListener("paused", async function(event, packet) {
      let {frames} = await threadClient.getFrames(0, 1);
      let frameLine = frames[0] ? frames[0].where.line : undefined;
      ok(frameLine == lineno, "Paused at line " + frameLine + " expected " + lineno);
    });
  };
}

// Define various methods that resume a thread in a specific way and ensure it
// pauses at a specified line.
var rewindToLine = resumeThenPauseAtLineFunctionFactory("rewind");
var resumeToLine = resumeThenPauseAtLineFunctionFactory("resume");
var reverseStepOverToLine = resumeThenPauseAtLineFunctionFactory("reverseStepOver");
var stepOverToLine = resumeThenPauseAtLineFunctionFactory("stepOver");
var reverseStepInToLine = resumeThenPauseAtLineFunctionFactory("reverseStepIn");
var stepInToLine = resumeThenPauseAtLineFunctionFactory("stepIn");
var reverseStepOutToLine = resumeThenPauseAtLineFunctionFactory("reverseStepOut");
var stepOutToLine = resumeThenPauseAtLineFunctionFactory("stepOut");

// Return a promise that resolves with the result of a thread evaluating a
// string in the topmost frame.
async function evaluateInTopFrame(threadClient, text) {
  let {frames} = await threadClient.getFrames(0, 1);
  ok(frames.length == 1, "Got one frame");
  let response = await threadClient.eval(frames[0].actor, text);
  ok(response.type == "resumed", "Got resume response from eval");
  let rval;
  await threadClient.addOneTimeListener("paused", function(event, packet) {
    ok(packet.type == "paused" &&
       packet.why.type == "clientEvaluated" &&
       "return" in packet.why.frameFinished, "Eval returned a value");
    rval = packet.why.frameFinished["return"];
  });
  return (rval.type == "undefined") ? undefined : rval;
}

// Return a promise that resolves when a thread evaluates a string in the
// topmost frame, ensuring the result matches the expected value.
async function checkEvaluateInTopFrame(threadClient, text, expected) {
  let rval = await evaluateInTopFrame(threadClient, text);
  ok(rval == expected, "Eval returned " + expected);
}

// Return a promise that resolves when a thread evaluates a string in the
// topmost frame, with the result throwing an exception.
async function checkEvaluateInTopFrameThrows(threadClient, text) {
  let {frames} = await threadClient.getFrames(0, 1);
  ok(frames.length == 1, "Got one frame");
  let response = await threadClient.eval(frames[0].actor, text);
  ok(response.type == "resumed", "Got resume response from eval");
  await threadClient.addOneTimeListener("paused", function(event, packet) {
    ok(packet.type == "paused" &&
       packet.why.type == "clientEvaluated" &&
       "throw" in packet.why.frameFinished, "Eval threw an exception");
  });
}

// Return a pathname that can be used for a new recording file.
function newRecordingFile() {
  ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
  return OS.Path.join(OS.Constants.Path.tmpDir,
                      "MochitestRecording" + Math.round(Math.random() * 1000000000));
}


async function warpToMessage(hud, threadClient, text) {
  let messages = await waitForMessages(hud, text);
  ok(messages.length == 1, "Found one message");
  let message = messages.pop();

  let menuPopup = await openConsoleContextMenu(hud, message);
  console.log(`.>> menu`, menuPopup);


  let timeWarpItem = menuPopup.querySelector("#console-menu-time-warp");
  ok(timeWarpItem, "Time warp menu item is available");

  timeWarpItem.click();

  await Promise.all([
    hideConsoleContextMenu(hud),
    once(Services.ppmm, "TimeWarpFinished"),
    waitForThreadEvents(threadClient, 'paused')
  ]);

  messages = findMessages(hud, "", ".paused");
  ok(messages.length == 1, "Found one paused message");

  return message;
}


function findMessage(hud, text, selector = ".message") {
  return findMessages(hud, text, selector)[0]
}

function findMessages(hud, text, selector = ".message") {
  const messages = hud.ui.outputNode.querySelectorAll(selector);
  const elements = Array.prototype.filter.call(
    messages,
    (el) => el.textContent.includes(text)
  );

  if (elements.length == 0) {
    return null;
  }

  return elements;
}

function waitForMessages(hud, text, selector = ".message") {
  return waitUntilPredicate(() => findMessages(hud, text, selector))
}

async function openConsoleContextMenu(hud, element) {
  const onConsoleMenuOpened = hud.ui.consoleOutput.once("menu-open");
  synthesizeContextMenuEvent(element);
  await onConsoleMenuOpened;
  const doc = hud.ui.consoleOutput.owner.chromeWindow.document;
  return doc.getElementById("webconsole-menu");
}

function hideConsoleContextMenu(hud) {
  const doc = hud.ui.consoleOutput.owner.chromeWindow.document;
  const popup = doc.getElementById("webconsole-menu");
  if (!popup) {
    return Promise.resolve();
  }

  const onPopupHidden = once(popup, "popuphidden");
  popup.hidePopup();
  return onPopupHidden;
}
